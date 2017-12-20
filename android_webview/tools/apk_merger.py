#!/usr/bin/python
# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

""" Merges a 64-bit and a 32-bit APK into a single APK

This script is used to merge two APKs which have only 32-bit or 64-bit
binaries respectively into a APK that has both 32-bit and 64-bit binaries
for 64-bit Android platform.

You normally don't need this script because GN 64-bit build generates
such APK for you.

To use this script, you need to
1. Build 32-bit APK as usual.
2. Build 64-bit APK with GN variable build_apk_secondary_abi=false.
3. Use this script to merge 2 APKs.

"""

import argparse
import collections
import filecmp
import os
import pprint
import re
import shutil
import sys
import tempfile
import zipfile

SRC_DIR = os.path.join(os.path.dirname(__file__), '..', '..')
SRC_DIR = os.path.abspath(SRC_DIR)
BUILD_ANDROID_GYP_DIR = os.path.join(SRC_DIR, 'build/android/gyp')
sys.path.append(BUILD_ANDROID_GYP_DIR)

import finalize_apk # pylint: disable=import-error
from util import build_utils # pylint: disable=import-error

class ApkMergeFailure(Exception):
  pass


def UnpackApk(file_name, dst):
  zippy = zipfile.ZipFile(file_name)
  zippy.extractall(dst)


def GetNonDirFiles(top, base_dir):
  """ Return a list containing all (non-directory) files in tree with top as
  root.

  Each file is represented by the relative path from base_dir to that file.
  If top is a file (not a directory) then a list containing only top is
  returned.
  """
  if os.path.isdir(top):
    ret = []
    for dirpath, _, filenames in os.walk(top):
      for filename in filenames:
        ret.append(
            os.path.relpath(os.path.join(dirpath, filename), base_dir))
    return ret
  else:
    return [os.path.relpath(top, base_dir)]


def GetDiffFiles(dcmp, base_dir):
  """ Return the list of files contained only in the right directory of dcmp.

  The files returned are represented by relative paths from base_dir.
  """
  copy_files = []
  for file_name in dcmp.right_only:
    copy_files.extend(
        GetNonDirFiles(os.path.join(dcmp.right, file_name), base_dir))

  # we cannot merge APKs with files with similar names but different contents
  if len(dcmp.diff_files) > 0:
    raise ApkMergeFailure('found differing files: %s in %s and %s' %
                          (dcmp.diff_files, dcmp.left, dcmp.right))

  if len(dcmp.funny_files) > 0:
    ApkMergeFailure('found uncomparable files: %s in %s and %s' %
                    (dcmp.funny_files, dcmp.left, dcmp.right))

  for sub_dcmp in dcmp.subdirs.itervalues():
    copy_files.extend(GetDiffFiles(sub_dcmp, base_dir))
  return copy_files


def CheckFilesExpected(actual_files, expected_files, component_build):
  """ Check that the lists of actual and expected files are the same. """
  actual_file_names = collections.defaultdict(int)
  for f in actual_files:
    actual_file_names[os.path.basename(f)] += 1
  actual_file_set = set(actual_file_names.iterkeys())
  expected_file_set = set(expected_files.iterkeys())

  unexpected_file_set = actual_file_set.difference(expected_file_set)
  if component_build:
    unexpected_file_set = set(
        f for f in unexpected_file_set if not f.endswith('.so'))
  missing_file_set = expected_file_set.difference(actual_file_set)
  duplicate_file_set = set(
      f for f, n in actual_file_names.iteritems() if n > 1)

  errors = []
  if unexpected_file_set:
    errors.append(
        '  unexpected files: %s' % pprint.pformat(unexpected_file_set))
  if missing_file_set:
    errors.append('  missing files: %s' % pprint.pformat(missing_file_set))
  if duplicate_file_set:
    errors.append('  duplicate files: %s' % pprint.pformat(duplicate_file_set))

  if errors:
    raise ApkMergeFailure(
        "Files don't match expectations:\n%s" % '\n'.join(errors))


def AddDiffFiles(diff_files, tmp_dir_32, out_zip, expected_files,
                 component_build, uncompress_shared_libraries):
  """ Insert files only present in 32-bit APK into 64-bit APK (tmp_apk). """
  for diff_file in diff_files:
    if component_build and diff_file.endswith('.so'):
      compress = not uncompress_shared_libraries
    else:
      compress = expected_files[os.path.basename(diff_file)]
    build_utils.AddToZipHermetic(out_zip,
                                 diff_file,
                                 os.path.join(tmp_dir_32, diff_file),
                                 compress=compress)


def SignAndAlignApk(tmp_apk, signed_tmp_apk, new_apk, zipalign_path,
                    keystore_path, key_name, key_password,
                    page_align_shared_libraries):
  try:
    finalize_apk.JarSigner(
        keystore_path,
        key_name,
        key_password,
        tmp_apk,
        signed_tmp_apk)
  except build_utils.CalledProcessError as e:
    raise ApkMergeFailure('Failed to sign APK: ' + e.output)

  try:
    finalize_apk.AlignApk(zipalign_path,
                          page_align_shared_libraries,
                          signed_tmp_apk,
                          new_apk)
  except build_utils.CalledProcessError as e:
    raise ApkMergeFailure('Failed to align APK: ' + e.output)

def GetSecondaryAbi(apk_zipfile, shared_library):
  ret = ''
  for name in apk_zipfile.namelist():
    if os.path.basename(name) == shared_library:
      abi = re.search('(^lib/)(.+)(/' + shared_library + '$)', name).group(2)
      # Intentionally not to add 64bit abi because they are not used.
      if abi == 'armeabi-v7a' or abi == 'armeabi':
        ret = 'arm64-v8a'
      elif abi == 'mips':
        ret = 'mips64'
      elif abi == 'x86':
        ret = 'x86_64'
      else:
        raise ApkMergeFailure('Unsupported abi ' + abi)
  if ret == '':
    raise ApkMergeFailure('Failed to find secondary abi')
  return ret

def MergeApk(args, tmp_apk, tmp_dir_32, tmp_dir_64):
  # Expected files to copy from 32- to 64-bit APK together with whether to
  # compress within the .apk.
  expected_files = {'snapshot_blob_32.bin': False}
  if args.shared_library:
    expected_files[args.shared_library] = not args.uncompress_shared_libraries
  if args.debug:
    expected_files['gdbserver'] = True

  # need to unpack APKs to compare their contents
  UnpackApk(args.apk_64bit, tmp_dir_64)
  UnpackApk(args.apk_32bit, tmp_dir_32)

  ignores = ['META-INF', 'AndroidManifest.xml']
  if args.ignore_classes_dex:
    ignores += ['classes.dex', 'classes2.dex']
  if args.debug:
    # see http://crbug.com/648720
    ignores += ['webview_licenses.notice']

  dcmp = filecmp.dircmp(
      tmp_dir_64,
      tmp_dir_32,
      ignore=ignores)

  diff_files = GetDiffFiles(dcmp, tmp_dir_32)

  # Check that diff_files match exactly those files we want to insert into
  # the 64-bit APK.
  CheckFilesExpected(diff_files, expected_files, args.component_build)

  with zipfile.ZipFile(tmp_apk, 'w') as out_zip:
    build_utils.MergeZips(out_zip, [args.apk_64bit],
                          exclude_patterns=['META-INF/*'])
    AddDiffFiles(diff_files, tmp_dir_32, out_zip, expected_files,
                 args.component_build, args.uncompress_shared_libraries)


def main():
  parser = argparse.ArgumentParser(
      description='Merge a 32-bit APK into a 64-bit APK')
  # Using type=os.path.abspath converts file paths to absolute paths so that
  # we can change working directory without affecting these paths
  parser.add_argument('--apk_32bit', required=True, type=os.path.abspath)
  parser.add_argument('--apk_64bit', required=True, type=os.path.abspath)
  parser.add_argument('--out_apk', required=True, type=os.path.abspath)
  parser.add_argument('--zipalign_path', required=True, type=os.path.abspath)
  parser.add_argument('--keystore_path', required=True, type=os.path.abspath)
  parser.add_argument('--key_name', required=True)
  parser.add_argument('--key_password', required=True)
  parser.add_argument('--shared_library')
  parser.add_argument('--page-align-shared-libraries', action='store_true')
  parser.add_argument('--uncompress-shared-libraries', action='store_true')
  parser.add_argument('--debug', action='store_true')
  # This option shall only used in debug build, see http://crbug.com/631494.
  parser.add_argument('--ignore-classes-dex', action='store_true')
  parser.add_argument('--component-build', action='store_true')
  args = parser.parse_args()

  tmp_dir = tempfile.mkdtemp()
  tmp_dir_64 = os.path.join(tmp_dir, '64_bit')
  tmp_dir_32 = os.path.join(tmp_dir, '32_bit')
  tmp_apk = os.path.join(tmp_dir, 'tmp.apk')
  signed_tmp_apk = os.path.join(tmp_dir, 'signed.apk')
  new_apk = args.out_apk

  try:
    if args.component_build and args.shared_library:
      raise ApkMergeFailure('--component-build and shared-library shouldn\'t'
                            ' be specified at same time.')
    if not args.component_build and not args.shared_library:
      raise ApkMergeFailure('Either --component-build or shared-library should'
                            ' be specified.')

    MergeApk(args, tmp_apk, tmp_dir_32, tmp_dir_64)

    SignAndAlignApk(tmp_apk, signed_tmp_apk, new_apk, args.zipalign_path,
                    args.keystore_path, args.key_name, args.key_password,
                    args.page_align_shared_libraries)

  except ApkMergeFailure as e:
    print e
    return 1
  finally:
    shutil.rmtree(tmp_dir)
  return 0


if __name__ == '__main__':
  sys.exit(main())
