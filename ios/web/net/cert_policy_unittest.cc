// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web/public/cert_policy.h"

#include "base/memory/ref_counted.h"
#include "net/cert/x509_certificate.h"
#include "net/test/test_certificate_data.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace web {

TEST(CertPolicyTest, Policy) {
  scoped_refptr<net::X509Certificate> google_cert(
      net::X509Certificate::CreateFromBytes(
          reinterpret_cast<const char*>(google_der), sizeof(google_der)));

  scoped_refptr<net::X509Certificate> webkit_cert(
      net::X509Certificate::CreateFromBytes(
          reinterpret_cast<const char*>(webkit_der), sizeof(webkit_der)));

  CertPolicy policy;

  // To begin with, everything should be unknown.
  EXPECT_EQ(CertPolicy::UNKNOWN,
            policy.Check(google_cert.get(), net::CERT_STATUS_DATE_INVALID));
  EXPECT_EQ(
      CertPolicy::UNKNOWN,
      policy.Check(webkit_cert.get(), net::CERT_STATUS_COMMON_NAME_INVALID));

  // Test adding one certificate with one error.
  policy.Allow(google_cert.get(), net::CERT_STATUS_DATE_INVALID);
  EXPECT_EQ(CertPolicy::ALLOWED,
            policy.Check(google_cert.get(), net::CERT_STATUS_DATE_INVALID));
  EXPECT_EQ(
      CertPolicy::UNKNOWN,
      policy.Check(google_cert.get(), net::CERT_STATUS_COMMON_NAME_INVALID));
  EXPECT_EQ(CertPolicy::UNKNOWN,
            policy.Check(google_cert.get(),
                         net::CERT_STATUS_DATE_INVALID |
                             net::CERT_STATUS_COMMON_NAME_INVALID));
  EXPECT_EQ(
      CertPolicy::UNKNOWN,
      policy.Check(webkit_cert.get(), net::CERT_STATUS_COMMON_NAME_INVALID));

  // Test saving the same certificate with a new error.
  policy.Allow(google_cert.get(), net::CERT_STATUS_AUTHORITY_INVALID);
  EXPECT_EQ(CertPolicy::UNKNOWN,
            policy.Check(google_cert.get(), net::CERT_STATUS_DATE_INVALID));
  EXPECT_EQ(
      CertPolicy::ALLOWED,
      policy.Check(google_cert.get(), net::CERT_STATUS_AUTHORITY_INVALID));
  EXPECT_EQ(
      CertPolicy::UNKNOWN,
      policy.Check(webkit_cert.get(), net::CERT_STATUS_COMMON_NAME_INVALID));

  // Test adding one certificate with two errors.
  policy.Allow(
      google_cert.get(),
      net::CERT_STATUS_DATE_INVALID | net::CERT_STATUS_AUTHORITY_INVALID);
  EXPECT_EQ(CertPolicy::ALLOWED,
            policy.Check(google_cert.get(), net::CERT_STATUS_DATE_INVALID));
  EXPECT_EQ(
      CertPolicy::ALLOWED,
      policy.Check(google_cert.get(), net::CERT_STATUS_AUTHORITY_INVALID));
  EXPECT_EQ(
      CertPolicy::UNKNOWN,
      policy.Check(google_cert.get(), net::CERT_STATUS_COMMON_NAME_INVALID));
  EXPECT_EQ(
      CertPolicy::UNKNOWN,
      policy.Check(webkit_cert.get(), net::CERT_STATUS_COMMON_NAME_INVALID));
}

}  // namespace web
