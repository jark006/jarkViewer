// Copyright 2019 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
// -----------------------------------------------------------------------------
//
// Cooperative multitasking (without threads):
// MainContext can yield control to LocalContext and vice-versa.
// It can be used to suspend and resume an algorithm without leaving the
// function or loop, avoiding the explicit data saving and loading.
// Nothing is done in parallel, this is just sequential context switching.
// Only available on Linux, Windows, Mac.
//
// Author: Yannis Guyon (yguyon@google.com)
//
// Code example:
//
//  bool RunSubAlgorithm(LocalContext* context, MySubDataType* my_sub_data) {
//    while (!my_sub_data->IsAvailable()) {  // As long as something's missing,
//      if (!context->Yield()) {  // Yield to main till it's available.
//        return false;           // Stop if early CloseLocalContext() or error.
//      }
//    }
//    // ... (do some work with my_sub_data)
//    return true;
//  }
//  void RunAlgorithm(LocalContext* context) {
//    MyDataType* my_data = (MyDataType*)context->GetInterContextData();
//    for (int i = 0; i < my_data->size(); ++i) {  // For all foreseeable data,
//      if (!RunSubAlgorithm(context, &my_data[i])) {  // Work on it.
//        break;                // Stop if early CloseLocalContext() or error.
//      }
//    }
//    context->Close();         // Don't forget to return to the main context.
//  }
//
//  MyDataType my_data;
//  MainContext main_context;
//  if (main_context.CreateLocalContext(&RunAlgorithm, &my_data)) {
//    while (main_context.Resume()) {     // As long as it's not finished,
//      my_data.ProvideMoreData();        // Make needed data available.
//    }
//    main_context.CloseLocalContext();  // Clean up.
//  }

#ifndef WP2_UTILS_CONTEXT_SWITCH_H_
#define WP2_UTILS_CONTEXT_SWITCH_H_

#if !defined(WP2_USE_CONTEXT_SWITCH)
#if !defined(ANDROID)
#if defined(__linux__) || defined(__APPLE__) || defined(_WIN32)
#define WP2_USE_CONTEXT_SWITCH 1
#endif
#else
// TODO(yguyon): fallback lib for Android?
#endif
#endif

#if defined(WP2_USE_CONTEXT_SWITCH) && (WP2_USE_CONTEXT_SWITCH > 0)

#if defined(_WIN32)
#include <Windows.h>
#undef Yield  // Empty macro defined in Windows.h.
#else
#if defined(__APPLE__)
#define _XOPEN_SOURCE
#endif
#include <ucontext.h>
#endif

#include <cstdint>

namespace WP2 {

//------------------------------------------------------------------------------
// LocalContext represent the environment of the function passed to
// MainContext::CreateLocalContext().

class LocalContext {
 private:
  LocalContext() = default;  // Created only by MainContext.

 public:
  LocalContext(const LocalContext&) = delete;
  LocalContext(LocalContext&&) = delete;
  virtual ~LocalContext() = default;

  bool Ok() const { return !error_; }

  // Retrieve user data passed to MainContext::CreateLocalContext().
  void* GetInterContextData() const { return inter_context_data_; }

  // Give back control to the main context. Returns false in case of early
  // MainContext::CloseLocalContext() or error (allocated memory should be freed
  // and LocalContext::Close() called as soon as possible).
  bool Yield();
  // Exit context. Must be called at the end of the function passed to
  // MainContext::CreateLocalContext(), and only there. All allocated memory
  // should be freed beforehand.
  // TODO(yguyon): Make an intermediate function that calls the user's function
  // then Close(). Makes it possible to use functions with custom parameters
  // (instead of inter_context_data) and avoids the risk of not calling Close().
  void Close();

 private:
  bool running_ = false;
  bool opened_ = false;
  bool closed_ = true;

  bool should_close_ = false;
  bool error_ = false;

  void* inter_context_data_ = nullptr;

#ifdef _WIN32
  // 1MB stack size is suggested in CreateFiber() documentation.
  static constexpr uint32_t kCallstackSize = (1u << 20);
  LPVOID local_fiber_ = nullptr;
  LPVOID main_fiber_ = nullptr;  // Doesn't have to be deleted.
#else
#ifdef WP2_BITTRACE
  // Keep a big stack for the ANS symbol debug map.
  static constexpr uint32_t kCallstackSize = (1u << 20);
#else
  // We are required to run with 64k stack (SIGSTKSZ is 8k, not enough).
  static constexpr uint32_t kCallstackSize = (1u << 16);
#endif
  ucontext_t local_context_;
  ucontext_t main_context_;
  uint8_t stack_buffer_[kCallstackSize];
#endif

  // Reset most variables to default values. Does not free anything nor yield.
  void Reset();
  friend class MainContext;
};

//------------------------------------------------------------------------------
// MainContext contains a reference to the local context.

class MainContext {
 public:
  MainContext() = default;
  MainContext(const MainContext&) = delete;
  MainContext(MainContext&&) = delete;
  virtual ~MainContext() { CloseLocalContext(); }

  bool Ok() const { return context_.Ok(); }

  // The suspendable_function must have a LocalContext* as only parameter.
  // It will be called at the first MainContext::Resume(), can suspend itself by
  // calling LocalContext::Yield(), and will continue its execution every time
  // MainContext::Resume() is called. It must end with a LocalContext::Close().
  // The 'inter_context_data' can be accessed with
  // LocalContext::GetInterContextData().
  bool CreateLocalContext(void (*suspendable_function)(void*),
                          void* inter_context_data);

  // Give back control to the local context. Returns false if local context
  // is closed or in case of error.
  bool Resume();
  // If any, resume the local context and fail all LocalContext::Yield()
  // till LocalContext::Close().
  void CloseLocalContext();

  // Returns true if nothing started yet or if it has been closed.
  bool IsLocalContextClosed() const { return context_.closed_; }

 private:
  LocalContext context_;
};

//------------------------------------------------------------------------------

}  // namespace WP2

#endif  // WP2_USE_CONTEXT_SWITCH

#endif /* WP2_UTILS_CONTEXT_SWITCH_H_ */
