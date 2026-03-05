// Copyright 2026 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/shell/browser/shell.h"
#include "cobalt/shell/browser/shell_test_support.h"
#include "content/public/browser/web_contents.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;

namespace content {

class LifecycleTest : public ShellTestBase {
 public:
  LifecycleTest() = default;

 protected:
  // Create a "fake" shell pointer by casting an arbitrary address.
  // We only use this pointer to register it in Shell::windows() so that
  // AppLifecycleDelegate can iterate over it and call Shell::RevealShell(shell), etc.
  // NOTE: ShellPlatformDelegate now has a safety check to skip 0x1234.
  Shell* CreateFakeShell() {
    Shell* fake_shell = reinterpret_cast<Shell*>(0x1234);
    Shell::windows().push_back(fake_shell);
    return fake_shell;
  }
};

TEST_F(LifecycleTest, StartupVisible) {
  InitializeShell(true);

  EXPECT_TRUE(platform_->IsVisible());
  EXPECT_EQ(platform_->application_state_for_testing(),
            ShellPlatformDelegate::kApplicationStateStarted);
}

TEST_F(LifecycleTest, StartupHidden) {
  InitializeShell(false);

  EXPECT_FALSE(platform_->IsVisible());
  EXPECT_EQ(platform_->application_state_for_testing(),
            ShellPlatformDelegate::kApplicationStateFrozen);
}

TEST_F(LifecycleTest, BlurFocus) {
  InitializeShell(true);
  CreateFakeShell();

  // Initial state is Started.
  EXPECT_EQ(platform_->application_state_for_testing(),
            ShellPlatformDelegate::kApplicationStateStarted);

  // Trigger blur.
  Shell::OnBlur();
  EXPECT_EQ(platform_->application_state_for_testing(),
            ShellPlatformDelegate::kApplicationStateBlurred);

  // Trigger focus.
  Shell::OnFocus();
  EXPECT_EQ(platform_->application_state_for_testing(),
            ShellPlatformDelegate::kApplicationStateStarted);
}

TEST_F(LifecycleTest, ConcealReveal) {
  InitializeShell(true);
  CreateFakeShell();

  EXPECT_TRUE(platform_->IsVisible());

  // Trigger conceal.
  Shell::OnConceal();

  EXPECT_FALSE(platform_->IsVisible());
  EXPECT_EQ(platform_->application_state_for_testing(),
            ShellPlatformDelegate::kApplicationStateConcealed);

  // Trigger reveal.
  Shell::OnReveal();

  EXPECT_TRUE(platform_->IsVisible());
  EXPECT_EQ(platform_->application_state_for_testing(),
            ShellPlatformDelegate::kApplicationStateBlurred);
}

TEST_F(LifecycleTest, RedundantReveal) {
  InitializeShell(true);
  CreateFakeShell();

  EXPECT_TRUE(platform_->IsVisible());

  // Trigger redundant reveal.
  Shell::OnReveal();

  EXPECT_TRUE(platform_->IsVisible());
}

TEST_F(LifecycleTest, FreezeUnfreeze) {
  InitializeShell(true);
  CreateFakeShell();

  // Started -> Concealed
  Shell::OnConceal();

  // Trigger freeze.
  Shell::OnFreeze();
  EXPECT_EQ(platform_->application_state_for_testing(),
            ShellPlatformDelegate::kApplicationStateFrozen);

  // Trigger unfreeze.
  Shell::OnUnfreeze();
  EXPECT_EQ(platform_->application_state_for_testing(),
            ShellPlatformDelegate::kApplicationStateConcealed);
}

TEST_F(LifecycleTest, FullLifecycle) {
  InitializeShell(true);
  CreateFakeShell();

  // Started -> Blurred
  Shell::OnBlur();
  EXPECT_EQ(platform_->application_state_for_testing(),
            ShellPlatformDelegate::kApplicationStateBlurred);

  // Blurred -> Started
  Shell::OnFocus();
  EXPECT_EQ(platform_->application_state_for_testing(),
            ShellPlatformDelegate::kApplicationStateStarted);

  // Started -> Concealed
  Shell::OnConceal();
  EXPECT_EQ(platform_->application_state_for_testing(),
            ShellPlatformDelegate::kApplicationStateConcealed);

  // Concealed -> Frozen
  Shell::OnFreeze();
  EXPECT_EQ(platform_->application_state_for_testing(),
            ShellPlatformDelegate::kApplicationStateFrozen);

  // Frozen -> Stopped
  Shell::OnStop();
  EXPECT_EQ(platform_->application_state_for_testing(),
            ShellPlatformDelegate::kApplicationStateStopped);

  // Attempting to move up from Stopped is not supported in this test (Stopped is terminal).
}

}  // namespace content
