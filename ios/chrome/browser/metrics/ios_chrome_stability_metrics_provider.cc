// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/metrics/ios_chrome_stability_metrics_provider.h"

IOSChromeStabilityMetricsProvider::IOSChromeStabilityMetricsProvider(
    PrefService* local_state)
    : helper_(local_state), recording_enabled_(false) {}

IOSChromeStabilityMetricsProvider::~IOSChromeStabilityMetricsProvider() {}

void IOSChromeStabilityMetricsProvider::OnRecordingEnabled() {
  recording_enabled_ = true;
}

void IOSChromeStabilityMetricsProvider::OnRecordingDisabled() {
  recording_enabled_ = false;
}

void IOSChromeStabilityMetricsProvider::ProvideStabilityMetrics(
    metrics::SystemProfileProto* system_profile_proto) {
  helper_.ProvideStabilityMetrics(system_profile_proto);
}

void IOSChromeStabilityMetricsProvider::ClearSavedStabilityMetrics() {
  helper_.ClearSavedStabilityMetrics();
}

void IOSChromeStabilityMetricsProvider::LogRendererCrash() {
  if (!recording_enabled_)
    return;

  // The actual termination code isn't provided on iOS; use a dummy value.
  // TODO(blundell): Think about having StabilityMetricsHelper have a variant
  // that doesn't supply these arguments to make this cleaner.
  int dummy_termination_code = 105;
  helper_.LogRendererCrash(false /* not an extension process */,
                           base::TERMINATION_STATUS_ABNORMAL_TERMINATION,
                           dummy_termination_code);
}

void IOSChromeStabilityMetricsProvider::WebStateDidStartLoading(
    web::WebState* web_state) {
  if (!recording_enabled_)
    return;

  helper_.LogLoadStarted();
}
