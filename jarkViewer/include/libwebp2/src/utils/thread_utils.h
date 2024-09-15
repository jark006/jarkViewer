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
// Multi-threaded worker
//
// Author: Skal (pascal.massimino@gmail.com)

#ifndef WP2_UTILS_THREAD_UTILS_H_
#define WP2_UTILS_THREAD_UTILS_H_

#include <cassert>
#include <cstdint>
#ifdef HAVE_CONFIG_H
#include "src/wp2/config.h"
#endif

#include "src/wp2/base.h"

namespace WP2 {

//------------------------------------------------------------------------------
// Generic mutex interface.

class ThreadLock {
 public:
  ThreadLock();
  ~ThreadLock();

  // Acquires the lock or returns an error. Waits if it is already acquired.
  WP2Status Acquire();
  void Release();

 private:
  struct Impl;
  Impl* impl_;
};

//------------------------------------------------------------------------------
// C++ Worker object with virtual method Execute()

class WorkerBase {
 public:
  WorkerBase() noexcept = default;
  WorkerBase(WorkerBase&&) noexcept = default;

  // To prevent copy (could potentially leave thread hanging or double-freed).
  WorkerBase(const WorkerBase&) = delete;

  virtual ~WorkerBase() {}

  // Main call. Should return the task status.
  virtual WP2Status Execute() = 0;
  // Calls Execute() in a thread or directly, depending on do_mt.
  WP2Status Start(bool do_mt, uint32_t stack_size = 0);
  // Waits for the thread to finish if do_mt was used. Returns Execute() status.
  WP2Status End();

  // State of the worker thread object
  enum {
    NOT_OK = 0,  // object is unusable
    OK,          // ready to work
    WORK         // busy finishing the current task
  } state_ = NOT_OK;

  WP2Status status_ = WP2_STATUS_OK;  // Should contain the output
                                      // of the last Execute() call.

  void SetImpl(void* impl) { impl_ = impl; }
  void* GetImpl() const { return impl_; }

 protected:
  void* impl_ = nullptr;  // platform-dependent implementation
};

//------------------------------------------------------------------------------
// Plain-C worker object with function pointer hook_ to execute

// Function to be called by the worker thread. Takes two opaque pointers as
// arguments (data1_ and data2_), and should return false in case of error.
using WorkerHook = WP2Status (*)(void*, void*);

// Synchronization object used to launch job in the worker thread
class Worker : public WorkerBase {
 public:
  WP2Status Execute() override {  // just calls the hook
    assert(status_ == WP2_STATUS_OK);
    return (hook_ != nullptr) ? hook_(data1_, data2_) : WP2_STATUS_OK;
  }

 public:
  WorkerHook hook_ = nullptr;  // hook to call
  void* data1_ = nullptr;      // first argument passed to 'hook_'
  void* data2_ = nullptr;      // second argument passed to 'hook_'
};

//------------------------------------------------------------------------------
// The interface for all thread-worker related functions. All these functions
// must be implemented.
class WorkerInterface {
 public:
  virtual ~WorkerInterface() = default;

  // Must be called to initialize the object and spawn the thread. Re-entrant,
  // but only needs to be called once to initialize the worker object.
  // Will potentially launch a waiting thread if 'do_mt' is true. do_mt can be
  // set to false in case the application wants to avoid using a thread. This
  // is only taken into account during the very first call to Reset() on this
  // worker. 'stack_size' is the limit in bytes for the thread's stack size.
  // Use a stack-size value of '0' to use the system's default size.
  // Returns false in case of error (memory or thread-launch).
  virtual bool Reset(WorkerBase* worker, bool do_mt,
                     uint32_t stack_size) const = 0;
  bool Reset(WorkerBase* const worker, bool do_mt) const {
    return Reset(worker, do_mt, 0);
  }
  // Triggers the thread to call worker's Execute(). If do_mt was false when
  // initially calling Reset(), the method is called directly, not in a thread.
  // But even in this case, it's still required to call Sync(), for proper
  // error reporting.
  virtual void Launch(WorkerBase* worker) const = 0;
  // Makes sure the previous work is finished. Returns true if worker->status_
  // is WP2_STATUS_OK and no error was triggered by the working thread.
  virtual WP2Status Sync(WorkerBase* worker) const = 0;
  // Kills the thread (if launched) and marks the object as closed. To use the
  // object again, one must call Reset() again.
  virtual void End(WorkerBase* worker) const = 0;
};

// Install a new set of threading functions, overriding the defaults. This
// should be done before any workers are started, i.e., before any encoding or
// decoding takes place. The contents of the interface object is NOT copied,
// but a pointer to it is kept. Hence, the interface must out-live the calls
// to the library. This function is not thread-safe.
WP2_EXTERN void SetWorkerInterface(const WorkerInterface& winterface);

// Retrieve the currently set thread worker interface.
WP2_EXTERN const WorkerInterface& GetWorkerInterface();

//------------------------------------------------------------------------------

}  // namespace WP2

#endif /* WP2_UTILS_THREAD_UTILS_H_ */
