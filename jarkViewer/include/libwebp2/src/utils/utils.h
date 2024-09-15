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
// Misc. common utility functions
//
// Authors: Skal (pascal.massimino@gmail.com)
//

#ifndef WP2_UTILS_UTILS_H_
#define WP2_UTILS_UTILS_H_

#ifdef HAVE_CONFIG_H
#include "src/wp2/config.h"
#endif

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <initializer_list>
#include <new>
#include <string>

#include "src/wp2/base.h"

//------------------------------------------------------------------------------
// Tracing and printing

#if defined(WP2_TRACE)
// we'll supply a default impl if WP2_TRACE is not defined
extern void WP2Trace(const char* format, const std::string prefix, ...);
#else
#define WP2Trace(format, ...) (void)format
#endif  // WP2_TRACE

// Safe replacement for printf(fmt, ...) when fmt is not a literal
void WP2Print(const char* fmt, ...);

// Safe replacement for sprintf(fmt, ...) when fmt is not a literal.
// Writes the data to str. Used only for debugging purposes, hence the
// replacement with an empty macro otherwise
#if defined(WP2_BITTRACE) || defined(WP2_TRACE) || defined(WP2_ENC_DEC_MATCH)
std::string WP2SPrint(const char* const fmt, ...);
#else
inline std::string WP2SPrint(const char* const fmt, ...) {
  (void)fmt;
  return std::string("");
}
#endif

// handy error-logging macros
void WP2ErrorLog(const char* file, int line, WP2Status s,
                 const char* extra_message = nullptr);

// performs "if (status != WP2_STATUS_OK) return status;"
#define WP2_CHECK_STATUS(status)                                 \
  do {                                                           \
    const WP2Status wp2_check_status_tmp_var = (status);         \
    if (wp2_check_status_tmp_var != WP2_STATUS_OK) {             \
      WP2ErrorLog(__FILE__, __LINE__, wp2_check_status_tmp_var); \
      return wp2_check_status_tmp_var;                           \
    }                                                            \
  } while (0)

// performs "if (!cond) return error_status;"
#define WP2_CHECK_OK(cond, error_status)             \
  do {                                               \
    if (!(cond)) {                                   \
      WP2ErrorLog(__FILE__, __LINE__, error_status); \
      return error_status;                           \
    }                                                \
  } while (0)

// specialized for vector alloc checks
#define WP2_CHECK_ALLOC_OK(cond) WP2_CHECK_OK(cond, WP2_STATUS_OUT_OF_MEMORY)

// performs "if (status != WP2_STATUS_OK) assert(false);"
#define WP2_ASSERT_STATUS(status)             \
  do {                                        \
    const WP2Status __S__ = (status);         \
    if (__S__ != WP2_STATUS_OK) {             \
      WP2ErrorLog(__FILE__, __LINE__, __S__); \
    }                                         \
    assert(__S__ == WP2_STATUS_OK);           \
  } while (0)

// Only performs the CHECK if WP2_REDUCE_BINARY_SIZE is *not* defined.
#if !defined(WP2_REDUCE_BINARY_SIZE)
#define WP2_CHECK_REDUCED_STATUS(status) WP2_CHECK_STATUS(status)
#else
#define WP2_CHECK_REDUCED_STATUS(status) \
  do {                                   \
  } while (false)
#endif

// Can be overridden and called instead of WP2ErrorLog() if 'wp2_error_tracer'
// is not null. Beware of concurrent accesses during multithreading.
#if defined(WP2_TRACE) || defined(WP2_ERROR_TRACE)
class WP2ErrorTracer {
 public:
  virtual ~WP2ErrorTracer() = default;
  virtual void Log(const char* const file, int line, WP2Status s,
                   const char* const extra_message);
};
extern WP2ErrorTracer* wp2_error_tracer;
#endif  // defined(WP2_TRACE) || defined(WP2_ERROR_TRACE)

//------------------------------------------------------------------------------
// Memory allocation

// This is the maximum memory amount that libwp2 will ever try to allocate.
#ifndef WP2_MAX_ALLOCABLE_MEMORY
#if SIZE_MAX > (1ULL << 34)
#define WP2_MAX_ALLOCABLE_MEMORY (1ULL << 34)
#else
// For 32-bit targets keep this below INT_MAX to avoid valgrind warnings.
#define WP2_MAX_ALLOCABLE_MEMORY ((1ULL << 31) - (1 << 16))
#endif
#endif  // WP2_MAX_ALLOCABLE_MEMORY

// This singleton class is used to wrap alloc/calloc/free into
// a customizable set of function hooks.
// Not necessarily thread-safe (although the default implementation is):
// if you change the library hook with WP2SetMemoryHook() while the library
// is being used, very bad things will happen!
// Subclass implementation should pay extra attention to mutex and locking.
struct WP2MemoryHook {
  // called for book-keeping purpose after WP2Malloc/WP2Calloc.
  // Should return 'false' if the allocation should be aborted.
  // Should *not* call Free() in case of abort.
  virtual bool Register(void* ptr, size_t size) { return ptr; }
  // called before a disallocation in WP2Free()
  virtual void Unregister(void* ptr) {}

  // customizable relays toward system calls.
  virtual void* Malloc(size_t size);
  virtual void* Calloc(size_t count, size_t size);
  virtual void Free(void* ptr);

  virtual ~WP2MemoryHook() = default;

  // returns false in case of size_t overflow of 'count * size' evaluation
  static bool CheckSizeArgumentsOverflow(uint64_t count, size_t size);
};

// Change the global memory hook used by the whole library.
// Should only be changed *before* any call to the library. Changing these
// functions in-flight *will* be painful! Please pay attention to any hidden
// or static allocations (in ctor, for instance) that may occur before you
// call this function to change the memory hooks!
// Returns the previous hook, or the currently set hook if 'hook' is null.
WP2MemoryHook* WP2SetMemoryHook(WP2MemoryHook* hook);

//------------------------------------------------------------------------------
// Memory functions

// Size-checking safe malloc/calloc: verify that the requested 'count * size' is
// not too large, or return nullptr.
// Note that these expect the second argument type to be 'size_t' in order to
// favor the "calloc(num_foo, sizeof(foo))" pattern.
WP2_EXTERN void* WP2Malloc(uint64_t count, size_t size);
WP2_EXTERN void* WP2Calloc(uint64_t count, size_t size);
// Releases memory allocated by the functions above or by WP2Decode*().
WP2_EXTERN void WP2Free(void* ptr);

// Inherit to call WP2Malloc and WP2Free on 'new' and 'delete'.
struct WP2Allocable {
  // Tag similar to std::nothrow.
  struct NoThrow {
  } static constexpr nothrow = NoThrow();  // NOLINT (Camelcase)

  // No virtual dtor to not force inherited objects to have a virtual table.

  // Enforce usage of 'WP2Allocable::nothrow' to warn the user.
  void* operator new(std::size_t size) = delete;
  void* operator new(std::size_t size, const std::nothrow_t&) = delete;
  void* operator new[](std::size_t size) = delete;
  void* operator new[](std::size_t size, const std::nothrow_t&) = delete;
  void* operator new(std::size_t size, const NoThrow&) noexcept {
    return WP2Malloc(1, size);
  }
  void* operator new[](std::size_t size, const NoThrow&) noexcept {
    return WP2Malloc(1, size);
  }
  void operator delete(void* ptr) noexcept { WP2Free(ptr); }
  void operator delete[](void* ptr) noexcept { WP2Free(ptr); }
  // Only called when a ctor throws during 'new (WP2Allocable::nothrow)'.
  void operator delete(void* ptr, const NoThrow&) noexcept { WP2Free(ptr); }
  void operator delete[](void* ptr, const NoThrow&) noexcept { WP2Free(ptr); }
};

//------------------------------------------------------------------------------

namespace WP2 {

// Convenient pointer/length tuple. Does not own or copy anything.
struct DataView {
  bool IsEmpty() const { return (size == 0); }

  const uint8_t* bytes;
  size_t size;
};

// Short move constructor. Should only be used for plain-old-data final classes
// that do not refer to themselves (pointer to another local member etc.).
template <typename T>
void TrivialMoveCtor(T* const created_instance, T* const moved_instance) {
  std::memcpy((void*)created_instance, (const void*)moved_instance, sizeof(T));
  new (moved_instance) T();
}

template <typename T>
void SetArray(T* array, const std::initializer_list<T>& values) {
  for (const T& value : values) *(array++) = value;
}

// Returns the length of a C array.
// Drop-in replacement for std::size() which is only available in C++17.
template <class ElementType, size_t FirstDimension>
constexpr std::size_t ArraySize(const ElementType (&)[FirstDimension]) {
  return FirstDimension;
}

#define STATIC_ASSERT_ARRAY_SIZE(array, size)                       \
  static_assert(WP2::ArraySize(array) == static_cast<size_t>(size), \
                "Expected " #array " to contain " #size " elements")

// Same as std::strstr() but considers 'str[count]' to be the null-character.
const char* strnstr(const char* str, size_t count, const char* target);

// Same as std::strncmp() but clamps 'count' to std::strlen(rhs).
int strlcmp(const char* lhs, const char* rhs, size_t count);

}  // namespace WP2

//------------------------------------------------------------------------------

#endif /* WP2_UTILS_UTILS_H_ */
