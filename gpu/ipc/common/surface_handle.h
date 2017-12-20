// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_IPC_COMMON_SURFACE_HANDLE_H_
#define GPU_IPC_COMMON_SURFACE_HANDLE_H_

#include <stdint.h>

#include "build/build_config.h"

#if (defined(OS_WIN) || defined(USE_X11) || defined(USE_OZONE)) && \
    !defined(OS_NACL)
#include "ui/gfx/native_widget_types.h"
#define GPU_SURFACE_HANDLE_IS_ACCELERATED_WINDOW
#endif

namespace gpu {

// SurfaceHandle is the native type used to reference a native surface in the
// GPU process so that we can create "view" contexts on it.

// On Windows, Linux and Chrome OS, we can use a AcceleratedWidget across
// processes, so SurfaceHandle is exactly that.
// On Mac and Android, there is no type we can directly access across processes,
// so we go through the GpuSurfaceTracker, and SurfaceHandle is a (scalar)
// handle generated by that.
// On NaCl, we don't have native surfaces per se, but we need SurfaceHandle to
// be defined, because some APIs that use it are referenced there.
#if defined(GPU_SURFACE_HANDLE_IS_ACCELERATED_WINDOW)
using SurfaceHandle = gfx::AcceleratedWidget;
constexpr SurfaceHandle kNullSurfaceHandle = gfx::kNullAcceleratedWidget;
#elif defined(OS_MACOSX) || defined(OS_ANDROID) || defined(OS_NACL)
using SurfaceHandle = int32_t;
constexpr SurfaceHandle kNullSurfaceHandle = 0;
#else
#error Platform not supported.
#endif

}  // namespace gpu

#endif  // GPU_IPC_COMMON_SURFACE_HANDLE_H_
