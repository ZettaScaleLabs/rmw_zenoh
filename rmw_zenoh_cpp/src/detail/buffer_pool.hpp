// Copyright 2024 Open Source Robotics Foundation, Inc.
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

#ifndef DETAIL__BUFFER_POOL_HPP_
#define DETAIL__BUFFER_POOL_HPP_

#include <mutex>
#include <cassert>
#include <vector>

#include "rcutils/allocator.h"

class BufferPool
{
public:
  BufferPool() = default;

  uint8_t * allocate(rcutils_allocator_t * allocator, size_t size)
  {
    // FIXME(fuzzypixelz): indeed, this methods leaks all allocated buffers ;)
    std::lock_guard<std::mutex> guard(mutex_);

    if (available_buffers_.empty()) {
      uint8_t * data = static_cast<uint8_t *>(allocator->allocate(size, allocator->state));
      assert(data);  // FIXME(fuzzypixelz): handle error
      SerializationBuffer buffer;
      buffer.data = data;
      buffer.size = size;
      buffers_.push_back(buffer);
      return data;
    } else {
      SerializationBuffer buffer = buffers_.at(available_buffers_.back());
      available_buffers_.pop_back();
      if (buffer.size < size) {
        buffer.data = static_cast<uint8_t *>(allocator->reallocate(
            buffer.data, size, allocator->state));
        assert(buffer.data);  // FIXME(fuzzypixelz): handle error
        buffer.size = size;
      }
      return buffer.data;
    }
  }

  void
  deallocate(uint8_t * data)
  {
    std::lock_guard<std::mutex> guard(mutex_);

    for (size_t i = 0; i < buffers_.size(); i++) {
      if (buffers_.at(i).data == data) {
        available_buffers_.push_back(i);
        return;
      }
    }
  }

private:
  struct SerializationBuffer
  {
    uint8_t * data;
    size_t size;
  };

  std::vector<SerializationBuffer> buffers_;
  std::vector<size_t> available_buffers_;
  // NOTE(fuzzypixelz): this will presumably locked in Zenoh's RX task and could cause deadlocks (?)
  std::mutex mutex_;
};

#endif  // DETAIL__BUFFER_POOL_HPP_
