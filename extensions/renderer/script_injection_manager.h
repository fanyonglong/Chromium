// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_SCRIPT_INJECTION_MANAGER_H_
#define EXTENSIONS_RENDERER_SCRIPT_INJECTION_MANAGER_H_

#include <stdint.h>

#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/scoped_observer.h"
#include "extensions/common/user_script.h"
#include "extensions/renderer/script_injection.h"
#include "extensions/renderer/user_script_set_manager.h"

struct ExtensionMsg_ExecuteCode_Params;

namespace content {
class RenderFrame;
}

namespace extensions {
class Extension;

// The ScriptInjectionManager manages extensions injecting scripts into frames
// via both content/user scripts and tabs.executeScript(). It is responsible for
// maintaining any pending injections awaiting permission or the appropriate
// load point, and injecting them when ready.
class ScriptInjectionManager : public UserScriptSetManager::Observer {
 public:
  explicit ScriptInjectionManager(
      UserScriptSetManager* user_script_set_manager);
  virtual ~ScriptInjectionManager();

  // Notifies that a new render view has been created.
  void OnRenderFrameCreated(content::RenderFrame* render_frame);

  // Removes pending injections of the unloaded extension.
  void OnExtensionUnloaded(const std::string& extension_id);

  void set_activity_logging_enabled(bool enabled) {
    activity_logging_enabled_ = enabled;
  }

 private:
  // A RenderFrameObserver implementation which watches the various render
  // frames in order to notify the ScriptInjectionManager of different
  // document load states and IPCs.
  class RFOHelper;

  using FrameStatusMap =
      std::map<content::RenderFrame*, UserScript::RunLocation>;

  using ScriptInjectionVector = std::vector<std::unique_ptr<ScriptInjection>>;

  // Notifies that an injection has been finished.
  void OnInjectionFinished(ScriptInjection* injection);

  // UserScriptSetManager::Observer implementation.
  void OnUserScriptsUpdated(const std::set<HostID>& changed_hosts) override;

  // Notifies that an RFOHelper should be removed.
  void RemoveObserver(RFOHelper* helper);

  // Invalidate any pending tasks associated with |frame|.
  void InvalidateForFrame(content::RenderFrame* frame);

  // Starts the process to inject appropriate scripts into |frame|.
  void StartInjectScripts(content::RenderFrame* frame,
                          UserScript::RunLocation run_location);

  // Actually injects the scripts into |frame|.
  void InjectScripts(content::RenderFrame* frame,
                     UserScript::RunLocation run_location);

  // Try to inject and store injection if it has not finished.
  void TryToInject(std::unique_ptr<ScriptInjection> injection,
                   UserScript::RunLocation run_location,
                   ScriptsRunInfo* scripts_run_info);

  // Handle the ExecuteCode extension message.
  void HandleExecuteCode(const ExtensionMsg_ExecuteCode_Params& params,
                         content::RenderFrame* render_frame);

  // Handle the ExecuteDeclarativeScript extension message.
  void HandleExecuteDeclarativeScript(content::RenderFrame* web_frame,
                                      int tab_id,
                                      const ExtensionId& extension_id,
                                      int script_id,
                                      const GURL& url);

  // Handle the GrantInjectionPermission extension message.
  void HandlePermitScriptInjection(int64_t request_id);

  // The map of active web frames to their corresponding statuses. The
  // RunLocation of the frame corresponds to the last location that has ran.
  FrameStatusMap frame_statuses_;

  // The frames currently being injected into, so long as that frame is valid.
  std::set<content::RenderFrame*> active_injection_frames_;

  // The collection of RFOHelpers.
  std::vector<std::unique_ptr<RFOHelper>> rfo_helpers_;

  // The set of UserScripts associated with extensions. Owned by the Dispatcher.
  UserScriptSetManager* user_script_set_manager_;

  // Pending injections which are waiting for either the proper run location or
  // user consent.
  ScriptInjectionVector pending_injections_;

  // Running injections which are waiting for async callbacks from blink.
  ScriptInjectionVector running_injections_;

  // Whether or not dom activity should be logged for scripts injected.
  bool activity_logging_enabled_ = false;

  ScopedObserver<UserScriptSetManager, UserScriptSetManager::Observer>
      user_script_set_manager_observer_;

  DISALLOW_COPY_AND_ASSIGN(ScriptInjectionManager);
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_SCRIPT_INJECTION_MANAGER_H_
