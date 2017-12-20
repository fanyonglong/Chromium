// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_IMMERSIVE_GESTURE_HANDLER_H_
#define ASH_WM_IMMERSIVE_GESTURE_HANDLER_H_

#include "ash/ash_export.h"

namespace ash {

// ImmersiveGestureHandler is responsible for calling
// ImmersiveFullscreenController::OnGestureEvent() when a gesture is received.
class ASH_EXPORT ImmersiveGestureHandler {
 public:
  virtual ~ImmersiveGestureHandler() {}
};

}  // namespace ash

#endif  // ASH_WM_IMMERSIVE_GESTURE_HANDLER_H_
