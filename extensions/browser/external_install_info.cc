// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/external_install_info.h"

#include "base/version.h"
#include "url/gurl.h"

namespace extensions {

ExternalInstallInfo::ExternalInstallInfo(const std::string& extension_id,
                                         int creation_flags,
                                         bool mark_acknowledged)
    : extension_id(extension_id),
      creation_flags(creation_flags),
      mark_acknowledged(mark_acknowledged) {}

ExternalInstallInfoFile::ExternalInstallInfoFile(
    const std::string& extension_id,
    std::unique_ptr<base::Version> version,
    const base::FilePath& path,
    Manifest::Location crx_location,
    int creation_flags,
    bool mark_acknowledged,
    bool install_immediately)
    : ExternalInstallInfo(extension_id, creation_flags, mark_acknowledged),
      version(std::move(version)),
      path(path),
      crx_location(crx_location),
      install_immediately(install_immediately) {}

ExternalInstallInfoFile::~ExternalInstallInfoFile() {}

ExternalInstallInfoUpdateUrl::ExternalInstallInfoUpdateUrl(
    const std::string& extension_id,
    const std::string& install_parameter,
    std::unique_ptr<GURL> update_url,
    Manifest::Location download_location,
    int creation_flags,
    bool mark_acknowledged)
    : ExternalInstallInfo(extension_id, creation_flags, mark_acknowledged),
      install_parameter(install_parameter),
      update_url(std::move(update_url)),
      download_location(download_location) {}

ExternalInstallInfoUpdateUrl::~ExternalInstallInfoUpdateUrl() {}

}  // namespace extensions
