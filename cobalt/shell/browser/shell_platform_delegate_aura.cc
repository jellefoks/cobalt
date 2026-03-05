// Copyright 2025 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "cobalt/shell/browser/shell_platform_delegate.h"

#include "base/command_line.h"
#include "cobalt/shell/browser/shell.h"
#include "cobalt/shell/browser/shell_platform_data_aura.h"
#include "cobalt/shell/common/shell_switches.h"
#include "content/public/browser/web_contents.h"
#include "ui/aura/window.h"

namespace content {

struct ShellPlatformDelegate::ShellData {
  // Aura-specific data for each Shell.
};

struct ShellPlatformDelegate::PlatformData {
  std::unique_ptr<ShellPlatformDataAura> aura_data;
};

ShellPlatformDelegate::ShellPlatformDelegate() {
  application_state_ = GetInitialApplicationState();
}

ShellPlatformDelegate::~ShellPlatformDelegate() = default;

void ShellPlatformDelegate::Initialize(const gfx::Size& default_window_size) {
  platform_ = std::make_unique<PlatformData>();
  platform_->aura_data =
      std::make_unique<ShellPlatformDataAura>(default_window_size);
}

void ShellPlatformDelegate::CreatePlatformWindow(
    Shell* shell,
    const gfx::Size& initial_size) {
  shell_data_map_[shell] = ShellData();
  if (IsVisible() && platform_->aura_data) {
    platform_->aura_data->Show();
  }
}

void ShellPlatformDelegate::CleanUp(Shell* shell) {
  shell_data_map_.erase(shell);
}

void ShellPlatformDelegate::DidCloseLastWindow() {}

void ShellPlatformDelegate::SetContents(Shell* shell) {
  if (platform_->aura_data) {
    platform_->aura_data->ResizeWindow(
        shell->web_contents()->GetContainerBounds().size());
  }
}

void ShellPlatformDelegate::LoadSplashScreenContents(Shell* shell) {}

void ShellPlatformDelegate::UpdateContents(Shell* shell) {}

void ShellPlatformDelegate::ResizeWebContent(Shell* shell,
                                             const gfx::Size& content_size) {
  if (platform_->aura_data) {
    platform_->aura_data->ResizeWindow(content_size);
  }
}

void ShellPlatformDelegate::EnableUIControl(Shell* shell,
                                            UIControl control,
                                            bool is_enabled) {}

void ShellPlatformDelegate::SetAddressBarURL(Shell* shell, const GURL& url) {}

void ShellPlatformDelegate::SetIsLoading(Shell* shell, bool loading) {}

void ShellPlatformDelegate::SetTitle(Shell* shell,
                                     const std::u16string& title) {
  if (platform_->aura_data) {
    platform_->aura_data->SetTitle(title);
  }
}

void ShellPlatformDelegate::MainFrameCreated(Shell* shell) {}

bool ShellPlatformDelegate::DestroyShell(Shell* shell) {
  return false;
}

void ShellPlatformDelegate::DidCreateOrAttachWebContents(
    Shell* shell,
    WebContents* web_contents) {}

std::unique_ptr<JavaScriptDialogManager>
ShellPlatformDelegate::CreateJavaScriptDialogManager(Shell* shell) {
  return nullptr;
}

bool ShellPlatformDelegate::HandleRequestToLockMouse(
    WebContents* web_contents,
    bool user_gesture,
    bool last_unlocked_by_target) {
  return false;
}

bool ShellPlatformDelegate::ShouldAllowRunningInsecureContent(Shell* shell) {
  return false;
}

void ShellPlatformDelegate::RunFileChooser(
    RenderFrameHost* render_frame_host,
    scoped_refptr<FileSelectListener> listener,
    const blink::mojom::FileChooserParams& params) {}

#if !BUILDFLAG(IS_ANDROID)
gfx::NativeWindow ShellPlatformDelegate::GetNativeWindow(Shell* shell) {
  if (platform_->aura_data) {
    return platform_->aura_data->host()->window();
  }
  return nullptr;
}
#endif

ShellPlatformDataAura* ShellPlatformDelegate::GetShellPlatformDataAura() {
  return platform_->aura_data.get();
}

void ShellPlatformDelegate::RevealShell(Shell* shell) {
  if (platform_ && platform_->aura_data) {
    platform_->aura_data->Show();
  }
}

void ShellPlatformDelegate::ConcealShell(Shell* shell) {
  if (platform_ && platform_->aura_data) {
    platform_->aura_data->Hide();
  }
}

}  // namespace content
