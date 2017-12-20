// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_SUGGEST_PERMISSION_UTIL_H_
#define EXTENSIONS_BROWSER_SUGGEST_PERMISSION_UTIL_H_

#include "extensions/common/permissions/api_permission.h"

namespace content {
class RenderFrameHost;
}

namespace extensions {

class Extension;

// Checks that |extension| is not NULL and that it has |permission|. If
// |extension| is NULL, just returns false. If an extension without |permission|
// returns false and suggests |permision| in the developer tools console.
bool IsExtensionWithPermissionOrSuggestInConsole(
    APIPermission::ID permission,
    const Extension* extension,
    content::RenderFrameHost* render_frame_host);

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_SUGGEST_PERMISSION_UTIL_H_
