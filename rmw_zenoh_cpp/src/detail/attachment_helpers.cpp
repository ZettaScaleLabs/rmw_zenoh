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

#include <zenoh.h>

#include <cstdlib>
#include <cstring>
#include <string>
#include <zenoh_macros.h>

#include "rmw/types.h"

#include "attachment_helpers.hpp"

namespace rmw_zenoh_cpp {

bool get_attachment(const z_loaned_bytes_t *const attachment,
                    const std::string &key, z_owned_string_t *val) {
  if (z_bytes_is_empty(attachment)) {
    printf("1\n");
    return false;
  }

  z_bytes_iterator_t iter = z_bytes_get_iterator(attachment);
  z_owned_bytes_t pair, key_, val_;
  bool found = false;

  while (z_bytes_iterator_next(&iter, &pair)) {

    printf("2\n");
    z_bytes_deserialize_into_pair(z_loan(pair), &key_, &val_);
    printf("3\n");

    z_owned_string_t key_string;
    z_bytes_deserialize_into_string(z_loan(key_), &key_string);
    printf("%s\n", z_string_data(z_loan(key_string)));
    if (strcmp(z_string_data(z_loan(key_string)), key.c_str()) == 0) {
      found = true;
    printf("4\n");
      z_bytes_deserialize_into_string(z_loan(val_), val);
      printf("5 Found %s\n", z_string_data(z_loan(*val)));
    }

    z_drop(z_move(pair));
    z_drop(z_move(key_));
    z_drop(z_move(val_));
    z_drop(z_move(key_string));

    if (found) {
      break;
    }
  }

  if (!found) {
    return false;
  }

  if (!z_string_check(val)) {
    return false;
  }

  return true;
}

bool get_gid_from_attachment(const z_loaned_bytes_t *const attachment,
                             uint8_t gid[RMW_GID_STORAGE_SIZE]) {

  z_owned_string_t index;
  if (!get_attachment(attachment, "source_gid", &index)) {
    z_drop(z_move(index));
    return false;
  }

  size_t len = z_string_len(z_loan(index));
  if (len != RMW_GID_STORAGE_SIZE) {
    return false;
  }

  const char *start = z_string_data(z_loan(index));
  memcpy(gid, start, len);

  z_drop(z_move(index));
  return true;
}

int64_t get_int64_from_attachment(const z_loaned_bytes_t *const attachment,
                                  const std::string &name) {
  // A valid request must have had an attachment
  if (z_bytes_is_empty(attachment)) {
    return -1;
  }

  z_owned_string_t index;
  if (!get_attachment(attachment,name, &index)) {
    z_drop(z_move(index));
    return -1;
  }

  size_t len = z_string_len(z_loan(index));
  if (len < 1) {
    return -1;
  }

  if (len > 19) {
    // The number was larger than we expected
    return -1;
  }

  // The largest possible int64_t number is INT64_MAX, i.e. 9223372036854775807.
  // That is 19 characters long, plus one for the trailing \0, means we need 20
  // bytes.
  char int64_str[20];

  memcpy(int64_str, z_string_data(z_loan(index)), len);
  int64_str[len] = '\0';

  errno = 0;
  char *endptr;
  int64_t num = strtol(int64_str, &endptr, 10);
  if (num == 0) {
    // This is an error regardless; the client should never send this
    return -1;
  } else if (endptr == int64_str) {
    // No values were converted, this is an error
    return -1;
  } else if (*endptr != '\0') {
    // There was junk after the number
    return -1;
  } else if (errno != 0) {
    // Some other error occurred, which may include overflow or underflow
    return -1;
  }

  return num;
}
} // namespace rmw_zenoh_cpp
