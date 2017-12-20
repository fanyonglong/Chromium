// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/public/provider/chrome/browser/voice/voice_search_language.h"

#include "base/mac/objc_property_releaser.h"

@implementation VoiceSearchLanguage {
  base::mac::ObjCPropertyReleaser _propertyReleaser_VoiceSearchLanguage;
}

@synthesize identifier = _identifier;
@synthesize displayName = _displayName;
@synthesize localizationPreference = _localizationPreference;

- (instancetype)initWithIdentifier:(NSString*)identifier
                       displayName:(NSString*)displayName
            localizationPreference:(NSString*)localizationPreference {
  if ((self = [super init])) {
    _identifier = [identifier copy];
    _displayName = [displayName copy];
    _localizationPreference = [localizationPreference copy];

    _propertyReleaser_VoiceSearchLanguage.Init(self,
                                               [VoiceSearchLanguage class]);
  }
  return self;
}

@end
