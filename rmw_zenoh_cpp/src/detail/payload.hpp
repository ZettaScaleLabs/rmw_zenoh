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

#ifndef DETAIL__PAYLOAD_HPP_
#define DETAIL__PAYLOAD_HPP_

#include <zenoh.h>

#include <variant>
#include <utility>

namespace rmw_zenoh_cpp
{
    ///=============================================================================
class Payload
{
public:
  explicit Payload(const z_loaned_bytes_t *bytes)
  {
    z_bytes_slice_iterator_t slices = z_bytes_get_slice_iterator(bytes);
    z_view_slice_t view;
    z_bytes_slice_iterator_next(&slices, &view);
    if (!z_bytes_slice_iterator_next(&slices, &view)) {
      z_owned_bytes_t owned_bytes;
      z_bytes_clone(&owned_bytes, bytes);
      Contiguous slice;
      slice.view = view;
      slice.owned_bytes = owned_bytes;
      bytes_ = slice;
    } else {
      z_owned_slice_t slice;
      z_bytes_to_slice(bytes, &slice);
      bytes_ = slice;
    }
  }

  ~Payload()
  {
    if (std::holds_alternative<NonContiguous>(bytes_)) {
      z_drop(z_move(std::get<NonContiguous>(bytes_)));
    } else {
      z_drop(z_move(std::get<Contiguous>(bytes_).owned_bytes));
    }
  }

  const uint8_t * data()
  {
    if (std::holds_alternative<NonContiguous>(bytes_)) {
      z_owned_slice_t owned = std::get<NonContiguous>(bytes_);
      return z_slice_data(z_loan(owned));
    } else {
      z_view_slice_t view = std::get<Contiguous>(bytes_).view;
      return z_slice_data(z_loan(view));
    }
  }

  size_t size()
  {
    if (std::holds_alternative<NonContiguous>(bytes_)) {
      z_owned_slice_t owned = std::get<NonContiguous>(bytes_);
      return z_slice_len(z_loan(owned));
    } else {
      z_view_slice_t view = std::get<Contiguous>(bytes_).view;
      return z_slice_len(z_loan(view));
    }
  }

private:
  struct Contiguous
  {
    z_view_slice_t view;
    z_owned_bytes_t owned_bytes;
  };
  using NonContiguous = z_owned_slice_t;
  // Is `z_owned_slice_t` in case of a non-contiguous `bytes`
  // and `z_view_slice_t` plus a `z_owned_bytes_t` otherwise.
  std::variant<NonContiguous, Contiguous> bytes_;
};
}  // namespace rmw_zenoh_cpp

#endif  // DETAIL__PAYLOAD_HPP_
