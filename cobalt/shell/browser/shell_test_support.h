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

#ifndef COBALT_SHELL_BROWSER_SHELL_TEST_SUPPORT_H_
#define COBALT_SHELL_BROWSER_SHELL_TEST_SUPPORT_H_

#include <memory>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "cobalt/shell/browser/shell.h"
#include "cobalt/shell/browser/shell_platform_delegate.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_content_client_initializer.h"
#include "mojo/core/embedder/embedder.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/test/aura_test_helper.h"
#include "ui/events/devices/device_data_manager.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gl/test/gl_surface_test_support.h"

namespace content {

class MockShellPlatformDelegate : public ShellPlatformDelegate {
 public:
  MockShellPlatformDelegate() {
    application_state_ = GetInitialApplicationState();
    ON_CALL(*this, RevealShell(::testing::_))
        .WillByDefault(::testing::Return());
    ON_CALL(*this, ConcealShell(::testing::_))
        .WillByDefault(::testing::Return());
    ON_CALL(*this, DoBlur()).WillByDefault(::testing::Return());
    ON_CALL(*this, DoFocus()).WillByDefault(::testing::Return());
    ON_CALL(*this, DoConceal()).WillByDefault(::testing::Return());
    ON_CALL(*this, DoReveal()).WillByDefault(::testing::Return());
    ON_CALL(*this, DoFreeze()).WillByDefault(::testing::Return());
    ON_CALL(*this, DoUnfreeze()).WillByDefault(::testing::Return());
    ON_CALL(*this, DoStop()).WillByDefault(::testing::Return());
  }
  ~MockShellPlatformDelegate() override = default;

  MOCK_METHOD(void, Initialize, (const gfx::Size&), (override));
  MOCK_METHOD(void,
              CreatePlatformWindow,
              (Shell * shell, const gfx::Size& initial_size),
              (override));
  MOCK_METHOD(void, CleanUp, (Shell * shell), (override));
  MOCK_METHOD(void, SetContents, (Shell * shell), (override));
  MOCK_METHOD(void,
              ResizeWebContent,
              (Shell * shell, const gfx::Size& content_size),
              (override));
  MOCK_METHOD(void,
              EnableUIControl,
              (Shell * shell, UIControl control, bool is_enabled),
              (override));
  MOCK_METHOD(void,
              SetAddressBarURL,
              (Shell * shell, const GURL& url),
              (override));
  MOCK_METHOD(void, SetIsLoading, (Shell * shell, bool loading), (override));
  MOCK_METHOD(void,
              SetTitle,
              (Shell * shell, const std::u16string& title),
              (override));
  MOCK_METHOD(void, DidCloseLastWindow, (), (override));
  MOCK_METHOD(bool, DestroyShell, (Shell * shell), (override));
  MOCK_METHOD(void, LoadSplashScreenContents, (Shell * shell), (override));
  MOCK_METHOD(void, UpdateContents, (Shell * shell), (override));

  MOCK_METHOD(void, RevealShell, (Shell * shell), (override));
  MOCK_METHOD(void, ConcealShell, (Shell * shell), (override));
  MOCK_METHOD(void, DoBlur, (), (override));
  MOCK_METHOD(void, DoFocus, (), (override));
  MOCK_METHOD(void, DoConceal, (), (override));
  MOCK_METHOD(void, DoReveal, (), (override));
  MOCK_METHOD(void, DoFreeze, (), (override));
  MOCK_METHOD(void, DoUnfreeze, (), (override));
  MOCK_METHOD(void, DoStop, (), (override));
};

struct MojoInitializer {
  MojoInitializer() {
    static bool initialized = false;
    if (!initialized) {
      gl::GLSurfaceTestSupport::InitializeOneOff();
      mojo::core::Init();
      initialized = true;
    }
    if (!ui::DeviceDataManager::HasInstance()) {
      ui::DeviceDataManager::CreateInstance();
    }
  }
};

class ShellTestBase : public ::testing::Test {
 public:
  ShellTestBase()
      : task_environment_(base::test::TaskEnvironment::MainThreadType::UI) {}
  ~ShellTestBase() override {
    Shell::windows().clear();
    Shell::Shutdown();
  }

  void SetUp() override {
    aura_test_helper_ = std::make_unique<aura::test::AuraTestHelper>();
    aura_test_helper_->SetUp();
    browser_context_ = std::make_unique<TestBrowserContext>();
  }

  void TearDown() override {
    browser_context_.reset();
    aura_test_helper_->TearDown();
    aura_test_helper_.reset();
  }

 protected:
  void InitializeShell(bool is_visible) {
    ShellPlatformDelegate::SetInitialPreload(!is_visible);
    auto platform =
        std::make_unique<::testing::NiceMock<MockShellPlatformDelegate>>();
    platform_ = platform.get();
    Shell::Initialize(std::move(platform), is_visible);
  }

  MojoInitializer mojo_initializer_;
  TestContentClientInitializer content_initializer_;
  BrowserTaskEnvironment task_environment_;
  std::unique_ptr<aura::test::AuraTestHelper> aura_test_helper_;
  std::unique_ptr<TestBrowserContext> browser_context_;
  raw_ptr<::testing::NiceMock<MockShellPlatformDelegate>> platform_ = nullptr;
};

}  // namespace content

#endif  // COBALT_SHELL_BROWSER_SHELL_TEST_SUPPORT_H_
