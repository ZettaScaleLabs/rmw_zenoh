// Copyright 2023 Open Source Robotics Foundation, Inc.
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

#include "zenoh_router_check.hpp"

#include <rcutils/env.h>
#include <rcutils/logging_macros.h>

#include <string>
#include <sstream>
#include <iomanip>

namespace
{

// Convert a Zenoh Id to a string
// Zenoh IDs are LSB-first 128bit unsigned and non-zero integers in hexadecimal lowercase.
// @param pid Zenoh Id to convert
std::string ZidToStr(z_id_t pid)
{
  std::stringstream ss;
  int len = 0;
  for (int i = 0; i < 16; i++) {
    if (pid.id[i]) {
      len = i + 1;
    }
  }
  if (!len) {
    ss << "";
  } else {
    for (int i = len - 1; i >= 0; --i) {
      ss << std::hex << std::setfill('0') << std::setw(2) << static_cast<int>(pid.id[i]);
    }
  }
  return ss.str();
}

}  // namespace

rmw_ret_t zenoh_router_check(z_session_t session)
{
  // Initialize context for callback
  void * context = malloc(sizeof(int));
  *(static_cast<int *>(context)) = 0;

  // Define callback
  auto callback = [](const struct z_id_t * id, void * ctx) {
      const std::string id_str = ZidToStr(*id);
      RCUTILS_LOG_INFO_NAMED(
        "ZenouRouterCheck",
        "A Zenoh router connected to the session with id '%s'", id_str.c_str());
      // Note: Callback is guaranteed to never be called
      // concurrently according to z_info_routers_zid docstring
      (*(static_cast<int *>(ctx)))++;
    };

  z_owned_closure_zid_t router_callback = z_closure(callback, nullptr /* drop */, context);
  z_info_routers_zid(session, z_move(router_callback));

  rmw_ret_t ret;
  if (*(static_cast<int *>(context)) == 0) {
    RCUTILS_LOG_ERROR_NAMED(
      "ZenouRouterCheck",
      "No Zenoh router connected to the session");
    ret = RMW_RET_ERROR;
  } else {
    RCUTILS_LOG_INFO_NAMED(
      "ZenouRouterCheck",
      "There are %d Zenoh routers connected to the session", *(static_cast<int *>(context)));

    ret = RMW_RET_OK;
  }

  // Not using the drop function from the closure as we want to keep the context for logging.
  free(context);
  return ret;
}
