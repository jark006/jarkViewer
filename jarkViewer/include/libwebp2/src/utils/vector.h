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
// Vector implementation
//
// Authors: Yannis Guyon (yguyon@google.com)

#ifndef WP2_UTILS_VECTOR_H_
#define WP2_UTILS_VECTOR_H_

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <type_traits>
#include <utility>

#include "src/utils/utils.h"
#include "src/wp2/base.h"

namespace WP2 {

//------------------------------------------------------------------------------

namespace Internal {

static constexpr size_t kMinVectorAllocation = 16;

// Returns the smallest power of two greater or equal to 'value'.
static size_t NextPow2(size_t value) {
  if (value == 0) return 0;
  --value;
  for (size_t i = 1; i < sizeof(size_t) * 8; i *= 2) value |= value >> i;
  return value + 1;
}

// Returns the smallest capacity greater or equal to 'value'.
static inline size_t NextCapacity(size_t value) {
  if (value == 0) return 0;
  if (value <= kMinVectorAllocation) return kMinVectorAllocation;
  return NextPow2(value);
}

//------------------------------------------------------------------------------
// Data structure equivalent to std::vector but returning false and to its last
// valid state on memory allocation failure.
// std::vector with a custom allocator does not fill this need without
// exceptions.

template <typename T>
class VectorBase {
 public:
  typedef T value_type;
  typedef T* iterator;
  typedef const T* const_iterator;

  VectorBase() noexcept = default;
  VectorBase(const VectorBase&) = delete;
  VectorBase(VectorBase&& other) noexcept { TrivialMoveCtor(this, &other); }
  ~VectorBase() {
    for (iterator it = begin(); it != end(); ++it) it->~T();
    WP2Free(items_);
  }

  // Reallocates just enough memory if needed so that 'new_cap' items can fit.
  WP2_NO_DISCARD bool reserve(size_t new_cap) {
    if (capacity_ < new_cap) {
      T* const new_items = (T*)WP2Malloc(new_cap, sizeof(T));
      if (new_items == nullptr) return false;
      move_items(new_cap, new_items);
    }
    return true;
  }

  // Reallocates less memory so that only the existing items can fit.
  WP2_NO_DISCARD bool shrink_to_fit() {
    if (capacity_ == num_items_) return true;
    if (num_items_ == 0) {
      WP2Free(items_);
      items_ = nullptr;
      capacity_ = 0;
      return true;
    }
    const size_t previous_capacity = capacity_;
    capacity_ = 0;  // Force reserve() to allocate and copy.
    if (reserve(num_items_)) return true;
    capacity_ = previous_capacity;
    return false;
  }
  // Sets the new size assuming there's enough capacity. Warning! No check.
  // Content is unmodified.
  void resize_no_check(size_t num_items) {
    if (capacity_ < num_items) assert(false);
    num_items_ = num_items;
  }

  // Constructs a new item by copy ctor. May reallocate if 'resize_if_needed'.
  WP2_NO_DISCARD bool push_back(const T& value, bool resize_if_needed = true) {
    if (num_items_ < capacity_) {
      new (&items_[num_items_]) T(value);
    } else {
      if (!resize_if_needed) return false;
      const size_t new_cap = Internal::NextCapacity(num_items_ + 1);
      T* const new_items = (T*)WP2Malloc(new_cap, sizeof(T));
      if (new_items == nullptr) return false;
      new (&new_items[num_items_]) T(value);
      move_items(new_cap, new_items);
    }
    ++num_items_;
    return true;
  }

  // Constructs a new item by move ctor. May reallocate if 'resize_if_needed'.
  WP2_NO_DISCARD bool push_back(T&& value, bool resize_if_needed = true) {
    if (num_items_ >= capacity_ &&
        (!resize_if_needed ||
         !reserve(Internal::NextCapacity(num_items_ + 1)))) {
      return false;
    }
    new (&items_[num_items_]) T(std::move(value));
    ++num_items_;
    return true;
  }

  inline void push_back_no_resize(const T& value) {
    if (!push_back(value, /*resize_if_needed=*/false)) assert(false);
  }
  inline void push_back_no_resize(T&& value) {
    if (!push_back(std::move(value), /*resize_if_needed=*/false)) assert(false);
  }

  // Destructs the last item.
  void pop_back() {
    --num_items_;
    items_[num_items_].~T();
  }

  // Destructs the item at 'pos'.
  void erase(iterator pos) { erase(pos, pos + 1); }

  // Destructs the items in [first,last).
  void erase(iterator first, iterator last) {
    for (iterator it = first; it != last; ++it) it->~T();
    if (last != end()) {
      if (std::is_trivial<T>::value) {
        std::memmove((void*)first, (const void*)last,
                     (end() - last) * sizeof(T));
      } else {
        for (iterator it_src = last, it_dst = first; it_src != end();
             ++it_src, ++it_dst) {
          new (it_dst) T(std::move(*it_src));
          it_src->~T();
        }
      }
    }
    num_items_ -= std::distance(first, last);
  }

  // Appends 'size' elements by copy. Returns false on error.
  WP2_NO_DISCARD bool append(const T data[], size_t size) {
    if (size == 0) return true;
    const size_t new_cap = Internal::NextCapacity(num_items_ + size);
    if (new_cap <= capacity_) {
      std::copy(data, data + size, &items_[num_items_]);
    } else {
      T* const new_items = (T*)WP2Malloc(new_cap, sizeof(T));
      if (new_items == nullptr) return false;
      std::copy(data, data + size, &new_items[num_items_]);
      move_items(new_cap, new_items);
    }
    num_items_ += size;
    return true;
  }

  // Destructs all the items.
  void clear() { erase(begin(), end()); }

  // Destroys (including deallocating) all the items.
  void reset() {
    clear();
    if (!shrink_to_fit()) assert(false);
  }

  // Accessors
  bool empty() const { return (num_items_ == 0); }
  size_t size() const { return num_items_; }
  size_t capacity() const { return capacity_; }

  T* data() { return items_; }
  T& front() { return items_[0]; }
  T& back() { return items_[num_items_ - 1]; }
  T& operator[](size_t i) { return items_[i]; }
  T& at(size_t i) { return items_[i]; }
  const T* data() const { return items_; }
  const T& front() const { return items_[0]; }
  const T& back() const { return items_[num_items_ - 1]; }
  const T& operator[](size_t i) const { return items_[i]; }
  const T& at(size_t i) const { return items_[i]; }

  iterator begin() { return &items_[0]; }
  const_iterator begin() const { return &items_[0]; }
  iterator end() { return &items_[num_items_]; }
  const_iterator end() const { return &items_[num_items_]; }

  void swap(VectorBase& b) {
    using std::swap;
    swap(items_, b.items_);
    swap(capacity_, b.capacity_);
    swap(num_items_, b.num_items_);
  }
  WP2_NO_DISCARD bool copy_from(const VectorBase& src) {
    if (src.items_ == items_) return true;
    clear();
    if (!reserve(src.capacity())) return false;
    for (const auto& elmt : src) {
      if (!push_back(elmt, false)) return false;
    }
    return true;
  }

 protected:
  T* items_ = nullptr;
  size_t capacity_ = 0;
  size_t num_items_ = 0;

 private:
  // Moves data from items_ into new_items, and have new_items be the new
  // items_.
  void move_items(size_t new_cap, T* new_items) {
    if (num_items_ > 0) {
      if (std::is_trivially_copyable<T>::value &&
          std::is_trivially_destructible<T>::value) {
        std::memcpy((void*)new_items, (const void*)items_,
                    num_items_ * sizeof(T));
      } else {
        for (size_t i = 0; i < num_items_; ++i) {
          new (&new_items[i]) T(std::move(items_[i]));
          items_[i].~T();
        }
      }
    }
    WP2Free(items_);
    items_ = new_items;
    capacity_ = new_cap;
  }
};

}  // namespace Internal

//------------------------------------------------------------------------------

// Vector class that does *NOT* construct/destruct the content on resize().
// Should be reserved to plain old data.
template <class T>
class VectorNoCtor : public Internal::VectorBase<T> {
  static_assert(std::is_trivially_destructible<T>::value,
                "Type should not have destructor.");

 public:
  // Creates or removes items so that 'new_num_items' exist.
  // Allocated memory grows every power-of-two items.
  WP2_NO_DISCARD bool resize(size_t new_num_items) {
    typedef Internal::VectorBase<T> super;
    if (!super::reserve(new_num_items)) return false;
    super::num_items_ = new_num_items;
    return true;
  }
};

// This generic vector class will call the constructors.
template <class T>
class Vector : public Internal::VectorBase<T> {
 public:
  // Constructs or destructs items so that 'new_num_items' exist.
  // Allocated memory grows every power-of-two items.
  WP2_NO_DISCARD bool resize(size_t new_num_items) {
    typedef Internal::VectorBase<T> super;
    if (super::num_items_ < new_num_items) {
      if (!super::reserve(new_num_items)) {
        return false;
      }
      while (super::num_items_ < new_num_items) {
        new (&super::items_[super::num_items_]) T();
        ++super::num_items_;
      }
    } else {
      while (super::num_items_ > new_num_items) {
        --super::num_items_;
        super::items_[super::num_items_].~T();
      }
    }
    return true;
  }
};

typedef VectorNoCtor<uint8_t> Vector_u8;
typedef VectorNoCtor<int8_t> Vector_s8;
typedef VectorNoCtor<uint16_t> Vector_u16;
typedef VectorNoCtor<int16_t> Vector_s16;
typedef VectorNoCtor<uint32_t> Vector_u32;
typedef VectorNoCtor<int32_t> Vector_s32;
typedef VectorNoCtor<uint64_t> Vector_u64;
typedef VectorNoCtor<int64_t> Vector_s64;
typedef VectorNoCtor<float> Vector_f;

//------------------------------------------------------------------------------

template <typename T>
void swap(VectorNoCtor<T>& a, VectorNoCtor<T>& b) {
  a.swap(b);
}

template <typename T>
void swap(Vector<T>& a, Vector<T>& b) {
  a.swap(b);
}

//------------------------------------------------------------------------------

}  // namespace WP2

#endif /* WP2_UTILS_VECTOR_H_ */
