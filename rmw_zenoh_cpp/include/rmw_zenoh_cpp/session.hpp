#include <rmw/types.h>
#include <zenoh.h>
#include "visibility_control.h"

#ifndef RMW_ZENOH_CPP__SESSION_HPP_
#define RMW_ZENOH_CPP__SESSION_HPP_

namespace rmw_zenoh_cpp
{
RMW_ZENOH_CPP_EXPORT
const z_loaned_session_t* get_zenoh_session(rmw_node_t * node);
} // namespace rmw_zenoh_cpp

#endif  // RMW_ZENOH_CPP__SESSION_HPP_
