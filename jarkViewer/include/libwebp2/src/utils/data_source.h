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
// DataSource is an abstraction of a source of data. Its actual implementation
// may be a static array, a stream etc.
//
// Author: Yannis Guyon (yguyon@google.com)

#ifndef WP2_UTILS_DATA_SOURCE_H_
#define WP2_UTILS_DATA_SOURCE_H_

#include <cassert>
#include <cstddef>
#include <cstdint>

namespace WP2 {

//------------------------------------------------------------------------------
// Inheritable class giving access to available data through TryGetNext().
// Fetch() is called when more data than GetNumNextBytes() is needed.
//
// Algorithms can access data with TryGetNext(), tell what they used with
// MarkNumBytesAsRead(), and higher program logic can decide whether to keep
// data (UnmarkAllReadBytes()) or discard it (Discard(GetNumReadBytes())).

class DataSource {
 protected:
  DataSource() = default;
  DataSource(const uint8_t* data, size_t data_size)
      : available_bytes_(data),
        num_available_bytes_(data_size),
        num_read_bytes_(0) {}

 public:
  DataSource(const DataSource&) = delete;  // Makes no sense to duplicate it.
  DataSource(DataSource&&) = default;
  virtual ~DataSource() = default;

  const DataSource& operator=(const DataSource&) = delete;
  DataSource& operator=(DataSource&&) = default;

  // Gets next available 'num_bytes' (or more) into '*data_pointer'; it might
  // Fetch() them. Returns false if it can not be fulfilled. Data pointer is
  // valid only until the next TryGetNext() call (internal buffer may be moved).
  bool TryGetNext(size_t num_bytes, const uint8_t** data_pointer);
  // Returns the maximum number of bytes that could have been successfully
  // returned by last TryGetNext() call (excluding read or discarded bytes).
  size_t GetNumNextBytes() const;
  // Returns how many bytes have been read so far (excluding discarded bytes).
  size_t GetNumReadBytes() const;
  // Returns how many bytes have been discarded so far.
  size_t GetNumDiscardedBytes() const;

  // Marks 'num_bytes' as read; following TryGetNext() results will be shifted
  // by 'num_bytes'.
  void MarkNumBytesAsRead(size_t num_bytes);
  // Resets 'num_read_bytes_' to 0.
  void UnmarkAllReadBytes();
  // Data can not be accessed again after it's been discarded.
  void Discard(size_t num_bytes);

  // Equivalent to TryGetNext() and, if successful, MarkNumBytesAsRead().
  // Optimized to do as few instructions as possible if there is enough data.
  inline bool TryReadNext(size_t num_bytes, const uint8_t** data_pointer) {
    assert(data_pointer != nullptr);
    *data_pointer = available_bytes_ + num_read_bytes_;
    num_read_bytes_ += num_bytes;
    if (num_read_bytes_ > num_available_bytes_) {
      num_read_bytes_ -= num_bytes;  // Fetch() might use GetNumNextBytes().
      if (!Fetch(num_bytes)) {
        *data_pointer = nullptr;
        return false;
      }
      // available_bytes_ might change during Fetch().
      *data_pointer = available_bytes_ + num_read_bytes_;
      num_read_bytes_ += num_bytes;
      // Fetch must return false if there isn't enough data.
      assert(num_read_bytes_ <= num_available_bytes_);
    }
    return true;
  }

  // Data pointer that stays valid even if the internal buffer gets reallocated.
  class DataHandle {
   public:
    DataHandle() = default;
    DataHandle(const DataSource* const data_source, size_t offset, size_t size)
        : data_source_(data_source), offset_(offset), size_(size) {}
    const uint8_t* GetBytes() const;
    size_t GetSize() const { return size_; }

   private:
    const DataSource* data_source_ = nullptr;
    size_t offset_ = 0;
    size_t size_ = 0;
  };

  // Returns a 'data_handle' that is valid until its bytes are discarded or
  // until this DataSource instance is destroyed.
  bool TryGetNext(size_t num_bytes, DataHandle* data_handle);
  bool TryReadNext(size_t num_bytes, DataHandle* data_handle);

  // Sets all members to default values.
  virtual void Reset();

 private:
  // Override and set 'available_bytes_', 'num_available_bytes_' with at least
  // 'num_read_bytes_ + num_requested_bytes' or return false.
  virtual bool Fetch(size_t num_requested_bytes) = 0;
  // Called after Discard() to signal that 'num_bytes' will not be used anymore
  // and can be freed. If there are more discarded bytes than available, the
  // difference must be discarded when there are new bytes. Remember to update
  // 'available_bytes_' and 'num_available_bytes_' if the buffer changed.
  virtual void OnDiscard(size_t num_bytes) = 0;

 protected:
  // Data is not owned, this is just a pointer to a part of an existing buffer.
  const uint8_t* available_bytes_ = nullptr;
  size_t num_available_bytes_ = 0;
  size_t num_read_bytes_ = 0;  // Number of available_bytes_ marked as read.
  size_t num_discarded_bytes_ = 0;
};

//------------------------------------------------------------------------------
// Use an existing buffer as a data source.
// ExternalDataSource makes it possible to Update() the buffer pointed by
// available_bytes_ and num_available_bytes_, when it gets reallocated or when
// more data is available.

class ExternalDataSource : public DataSource {
 public:
  ExternalDataSource() noexcept : DataSource() {}
  ExternalDataSource(const uint8_t* data, size_t data_size)
      : DataSource(data, data_size) {}

  void Update(const uint8_t* data, size_t data_size);

 private:
  bool Fetch(size_t num_requested_bytes) override;
  void OnDiscard(size_t num_bytes) override;
};

//------------------------------------------------------------------------------

}  // namespace WP2

#endif /* WP2_UTILS_DATA_SOURCE_H_ */
