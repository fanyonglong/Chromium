// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/system/chromeos/power/dual_role_notification.h"

#include <set>

#include "ash/common/system/chromeos/power/power_status.h"
#include "ash/common/system/system_notifier.h"
#include "ash/common/system/tray/system_tray_controller.h"
#include "ash/common/wm_shell.h"
#include "base/strings/utf_string_conversions.h"
#include "grit/ash_resources.h"
#include "grit/ash_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/time_format.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/notification.h"
#include "ui/message_center/notification_delegate.h"

using message_center::MessageCenter;
using message_center::Notification;

namespace ash {
namespace {

const char kDualRoleNotificationId[] = "dual-role";

// Opens power settings on click.
class DualRoleNotificationDelegate
    : public message_center::NotificationDelegate {
 public:
  DualRoleNotificationDelegate() {}

  // Overridden from message_center::NotificationDelegate.
  void Click() override {
    WmShell::Get()->system_tray_controller()->ShowPowerSettings();
  }

 private:
  ~DualRoleNotificationDelegate() override {}

  DISALLOW_COPY_AND_ASSIGN(DualRoleNotificationDelegate);
};

}  // namespace

DualRoleNotification::DualRoleNotification(MessageCenter* message_center)
    : message_center_(message_center),
      num_dual_role_sinks_(0),
      line_power_connected_(false) {}

DualRoleNotification::~DualRoleNotification() {
  if (message_center_->FindVisibleNotificationById(kDualRoleNotificationId))
    message_center_->RemoveNotification(kDualRoleNotificationId, false);
}

void DualRoleNotification::Update() {
  const PowerStatus& status = *PowerStatus::Get();
  DCHECK(status.HasDualRoleDevices());

  std::string current_power_source_id = status.GetCurrentPowerSourceID();

  std::unique_ptr<PowerStatus::PowerSource> new_source;
  std::unique_ptr<PowerStatus::PowerSource> new_sink;
  size_t num_sinks_found = 0;
  for (const auto& source : status.GetPowerSources()) {
    // The power source can't be changed if there's a dedicated charger.
    if (source.type == PowerStatus::DEDICATED_CHARGER) {
      dual_role_source_.reset();
      line_power_connected_ = true;
      if (message_center_->FindVisibleNotificationById(kDualRoleNotificationId))
        message_center_->RemoveNotification(kDualRoleNotificationId, false);
      return;
    }

    if (source.id == current_power_source_id) {
      new_source.reset(new PowerStatus::PowerSource(source));
      continue;
    }
    num_sinks_found++;
    // The notification only shows the sink port if it is the only sink.
    if (num_sinks_found == 1)
      new_sink.reset(new PowerStatus::PowerSource(source));
    else
      new_sink.reset();
  }

  // Check if the notification should change.
  bool change = false;
  if (PowerStatus::Get()->IsLinePowerConnected() != line_power_connected_) {
    change = true;
    line_power_connected_ = PowerStatus::Get()->IsLinePowerConnected();
  } else if (new_source && dual_role_source_) {
    if (new_source->description_id != dual_role_source_->description_id)
      change = true;
  } else if (new_source || dual_role_source_) {
    change = true;
  } else {
    // Notification differs for 0, 1, and 2+ sinks.
    if ((num_sinks_found < num_dual_role_sinks_ && num_sinks_found < 2) ||
        (num_sinks_found > num_dual_role_sinks_ && num_dual_role_sinks_ < 2)) {
      change = true;
    } else if (num_sinks_found == 1) {
      // The description matters if there's only one dual-role device.
      change = new_sink->description_id != dual_role_sink_->description_id;
    }
  }

  dual_role_source_ = std::move(new_source);
  dual_role_sink_ = std::move(new_sink);
  num_dual_role_sinks_ = num_sinks_found;

  if (!change)
    return;

  if (!message_center_->FindVisibleNotificationById(kDualRoleNotificationId)) {
    message_center_->AddNotification(CreateNotification());
  } else {
    message_center_->UpdateNotification(kDualRoleNotificationId,
                                        CreateNotification());
  }
}

std::unique_ptr<Notification> DualRoleNotification::CreateNotification() {
  base::string16 title;
  if (dual_role_source_) {
    title = l10n_util::GetStringFUTF16(
        IDS_ASH_STATUS_TRAY_CHARGING_FROM_DUAL_ROLE_TITLE,
        l10n_util::GetStringUTF16(dual_role_source_->description_id));
  } else if (num_dual_role_sinks_ == 1) {
    title = l10n_util::GetStringFUTF16(
        IDS_ASH_STATUS_TRAY_CHARGING_DUAL_ROLE_DEVICE_TITLE,
        l10n_util::GetStringUTF16(dual_role_sink_->description_id));
  } else {
    title = l10n_util::GetStringUTF16(
        IDS_ASH_STATUS_TRAY_CHARGING_DUAL_ROLE_DEVICES_TITLE);
  }

  std::unique_ptr<Notification> notification(new Notification(
      message_center::NOTIFICATION_TYPE_SIMPLE, kDualRoleNotificationId, title,
      l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_DUAL_ROLE_MESSAGE),
      ui::ResourceBundle::GetSharedInstance().GetImageNamed(
          IDR_AURA_NOTIFICATION_LOW_POWER_CHARGER),
      base::string16(), GURL(),
      message_center::NotifierId(message_center::NotifierId::SYSTEM_COMPONENT,
                                 system_notifier::kNotifierDualRole),
      message_center::RichNotificationData(),
      new DualRoleNotificationDelegate));
  notification->set_priority(message_center::MIN_PRIORITY);
  return notification;
}

}  // namespace ash
