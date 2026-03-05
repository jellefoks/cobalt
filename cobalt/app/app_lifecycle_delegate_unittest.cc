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

#include "cobalt/app/app_lifecycle_delegate.h"

#include "cobalt/shell/browser/shell.h"
#include "cobalt/shell/browser/shell_test_support.h"
#include "starboard/event.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::Return;

namespace cobalt {

class MockAppLifecycleRunner : public AppLifecycleRunner {
 public:
  MockAppLifecycleRunner() = default;
  ~MockAppLifecycleRunner() override = default;

  MOCK_METHOD(void, InitializeSystem, (), (override));
  MOCK_METHOD(void,
              CreateMainDelegate,
              (absl::optional<int64_t> startup_timestamp, bool is_visible),
              (override));
  MOCK_METHOD(cobalt::CobaltMainDelegate*, GetMainDelegate, (), (override));
  MOCK_METHOD(int, Run, (content::ContentMainParams), (override));
  MOCK_METHOD(void, ShutDown, (), (override));
  MOCK_METHOD(bool, IsRunning, (), (const, override));
};

class AppLifecycleDelegateTest : public content::ShellTestBase {
 public:
  AppLifecycleDelegateTest() {
    auto runner = std::make_unique<MockAppLifecycleRunner>();
    runner_ = runner.get();
    delegate_ = std::make_unique<AppLifecycleDelegate>(std::move(runner));
  }

 protected:
  void SendStartEvent(bool is_visible) {
    EXPECT_CALL(*runner_, InitializeSystem());
    EXPECT_CALL(*runner_, CreateMainDelegate(_, is_visible));
    EXPECT_CALL(*runner_, GetMainDelegate()).WillOnce(Return(nullptr));
    EXPECT_CALL(*runner_, Run(_)).WillOnce(Return(0));

    SbEventStartData data = {nullptr, 0, nullptr};
    SbEvent event = {is_visible ? kSbEventTypeStart : kSbEventTypePreload, 0,
                     &data};
    delegate_->HandleEvent(&event);
  }

  void SendEvent(SbEventType type) {
    SbEvent event = {type, 0, nullptr};
    delegate_->HandleEvent(&event);
  }

  MockAppLifecycleRunner* runner_;
  std::unique_ptr<AppLifecycleDelegate> delegate_;
};

TEST_F(AppLifecycleDelegateTest, StartVisible) {
  InitializeShell(true);
  SendStartEvent(true);

  EXPECT_CALL(*runner_, IsRunning()).WillOnce(Return(true));
  EXPECT_TRUE(delegate_->IsRunning());
  EXPECT_TRUE(platform_->IsVisible());
  EXPECT_EQ(platform_->application_state_for_testing(),
            content::ShellPlatformDelegate::kApplicationStateStarted);
}

TEST_F(AppLifecycleDelegateTest, StartPreloaded) {
  InitializeShell(false);
  SendStartEvent(false);

  EXPECT_CALL(*runner_, IsRunning()).WillOnce(Return(true));
  EXPECT_TRUE(delegate_->IsRunning());
  EXPECT_FALSE(platform_->IsVisible());
  EXPECT_EQ(platform_->application_state_for_testing(),
            content::ShellPlatformDelegate::kApplicationStateFrozen);
}

TEST_F(AppLifecycleDelegateTest, BlurFocus) {
  InitializeShell(true);
  SendStartEvent(true);

  // Transition: Started -> Blurred
  SendEvent(kSbEventTypeBlur);
  EXPECT_EQ(platform_->application_state_for_testing(),
            content::ShellPlatformDelegate::kApplicationStateBlurred);

  // Transition: Blurred -> Started
  SendEvent(kSbEventTypeFocus);
  EXPECT_EQ(platform_->application_state_for_testing(),
            content::ShellPlatformDelegate::kApplicationStateStarted);
}

TEST_F(AppLifecycleDelegateTest, ConcealReveal) {
  InitializeShell(true);
  SendStartEvent(true);

  // Transition: Started -> Blurred -> Concealed
  SendEvent(kSbEventTypeConceal);
  EXPECT_EQ(platform_->application_state_for_testing(),
            content::ShellPlatformDelegate::kApplicationStateConcealed);

  // Transition: Concealed -> Blurred
  SendEvent(kSbEventTypeReveal);
  EXPECT_EQ(platform_->application_state_for_testing(),
            content::ShellPlatformDelegate::kApplicationStateBlurred);
}

TEST_F(AppLifecycleDelegateTest, FreezeUnfreeze) {
  InitializeShell(false);
  SendStartEvent(false);

  // Transition: Frozen -> Concealed
  SendEvent(kSbEventTypeUnfreeze);
  EXPECT_EQ(platform_->application_state_for_testing(),
            content::ShellPlatformDelegate::kApplicationStateConcealed);

  // Transition: Concealed -> Frozen
  SendEvent(kSbEventTypeFreeze);
  EXPECT_EQ(platform_->application_state_for_testing(),
            content::ShellPlatformDelegate::kApplicationStateFrozen);
}

TEST_F(AppLifecycleDelegateTest, Stop) {
  InitializeShell(true);
  SendStartEvent(true);

  EXPECT_CALL(*runner_, IsRunning()).WillOnce(Return(true));
  EXPECT_CALL(*runner_, ShutDown());
  SendEvent(kSbEventTypeStop);

  EXPECT_CALL(*runner_, IsRunning()).WillOnce(Return(false));
  EXPECT_FALSE(delegate_->IsRunning());
  EXPECT_EQ(platform_->application_state_for_testing(),
            content::ShellPlatformDelegate::kApplicationStateStopped);
}

TEST_F(AppLifecycleDelegateTest, FullLifecycle) {
  InitializeShell(true);
  SendStartEvent(true);

  // Focus -> Blur
  SendEvent(kSbEventTypeBlur);
  EXPECT_EQ(platform_->application_state_for_testing(),
            content::ShellPlatformDelegate::kApplicationStateBlurred);

  // Blur -> Focus
  SendEvent(kSbEventTypeFocus);
  EXPECT_EQ(platform_->application_state_for_testing(),
            content::ShellPlatformDelegate::kApplicationStateStarted);

  // Focus -> Conceal
  SendEvent(kSbEventTypeConceal);
  EXPECT_EQ(platform_->application_state_for_testing(),
            content::ShellPlatformDelegate::kApplicationStateConcealed);

  // Concealed -> Freeze
  SendEvent(kSbEventTypeFreeze);
  EXPECT_EQ(platform_->application_state_for_testing(),
            content::ShellPlatformDelegate::kApplicationStateFrozen);

  // Frozen -> Unfreeze
  SendEvent(kSbEventTypeUnfreeze);
  EXPECT_EQ(platform_->application_state_for_testing(),
            content::ShellPlatformDelegate::kApplicationStateConcealed);

  // Concealed -> Reveal
  SendEvent(kSbEventTypeReveal);
  EXPECT_EQ(platform_->application_state_for_testing(),
            content::ShellPlatformDelegate::kApplicationStateBlurred);

  // Blurred -> Focus
  SendEvent(kSbEventTypeFocus);
  EXPECT_EQ(platform_->application_state_for_testing(),
            content::ShellPlatformDelegate::kApplicationStateStarted);

  // Finally Stop
  EXPECT_CALL(*runner_, IsRunning()).WillOnce(Return(true));
  EXPECT_CALL(*runner_, ShutDown());
  SendEvent(kSbEventTypeStop);
  EXPECT_EQ(platform_->application_state_for_testing(),
            content::ShellPlatformDelegate::kApplicationStateStopped);
}

}  // namespace cobalt
