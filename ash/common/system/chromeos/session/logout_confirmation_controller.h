// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMMON_SYSTEM_CHROMEOS_SESSION_LOGOUT_CONFIRMATION_CONTROLLER_H_
#define ASH_COMMON_SYSTEM_CHROMEOS_SESSION_LOGOUT_CONFIRMATION_CONTROLLER_H_

#include <memory>

#include "ash/ash_export.h"
#include "ash/common/shell_observer.h"
#include "ash/common/system/chromeos/session/last_window_closed_observer.h"
#include "base/callback.h"
#include "base/macros.h"
#include "base/time/time.h"
#include "base/timer/timer.h"

namespace base {
class TickClock;
}

namespace ash {

class LogoutConfirmationDialog;

// This class shows a dialog asking the user to confirm or deny logout and
// terminates the session if the user either confirms or allows the countdown
// shown in the dialog to expire.
//
// It is guaranteed that no more than one confirmation dialog will be visible at
// any given time. If there are multiple requests to show a confirmation dialog
// at the same time, the dialog whose countdown expires first is shown.
//
// In public sessions, asks the user to end the session when the last window is
// closed.
class ASH_EXPORT LogoutConfirmationController
    : public ShellObserver,
      public LastWindowClosedObserver {
 public:
  // The |logout_closure| must be safe to call as long as |this| is alive.
  explicit LogoutConfirmationController(const base::Closure& logout_closure);
  ~LogoutConfirmationController() override;

  base::TickClock* clock() const { return clock_.get(); }

  // Shows a LogoutConfirmationDialog. If a confirmation dialog is already being
  // shown, it is closed and a new one opened if |logout_time| is earlier than
  // the current dialog's |logout_time_|.
  void ConfirmLogout(base::TimeTicks logout_time);

  void SetClockForTesting(std::unique_ptr<base::TickClock> clock);

  // ShellObserver:
  void OnLockStateChanged(bool locked) override;

  // Called by the |dialog_| when the user confirms logout.
  void OnLogoutConfirmed();

  // Called by the |dialog_| when it is closed.
  void OnDialogClosed();

  LogoutConfirmationDialog* dialog_for_testing() const { return dialog_; }

 private:
  // LastWindowClosedObserver:
  void OnLastWindowClosed() override;

  std::unique_ptr<base::TickClock> clock_;
  base::Closure logout_closure_;

  base::TimeTicks logout_time_;
  LogoutConfirmationDialog* dialog_;  // Owned by the Views hierarchy.
  base::Timer logout_timer_;

  DISALLOW_COPY_AND_ASSIGN(LogoutConfirmationController);
};

}  // namespace ash

#endif  // ASH_COMMON_SYSTEM_CHROMEOS_SESSION_LOGOUT_CONFIRMATION_CONTROLLER_H_
