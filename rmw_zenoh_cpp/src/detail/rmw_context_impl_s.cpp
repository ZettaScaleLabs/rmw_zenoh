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

#include "rmw_context_impl_s.hpp"

#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <utility>

#include "guard_condition.hpp"
#include "identifier.hpp"
#include "liveliness_utils.hpp"
#include "logging_macros.hpp"
#include "zenoh_config.hpp"
#include "zenoh_router_check.hpp"

#include "rcpputils/scope_exit.hpp"
#include "rmw/error_handling.h"
#include "rmw/impl/cpp/macros.hpp"

// Megabytes of SHM to reserve.
// TODO(clalancette): Make this configurable, or get it from the configuration
#define SHM_BUFFER_SIZE_MB 10

///=============================================================================
void rmw_context_impl_s::graph_sub_data_handler(z_loaned_sample_t * sample, void * data)
{
  z_view_string_t keystr;
  z_keyexpr_as_view_string(z_sample_keyexpr(sample), &keystr);

  auto data_ptr = static_cast<Data *>(data);
  if (data_ptr == nullptr) {
    RMW_ZENOH_LOG_ERROR_NAMED(
      "rmw_zenoh_cpp",
      "[graph_sub_data_handler] Invalid data_ptr."
    );
    return;
  }

  // Update the graph cache.
  std::lock_guard<std::mutex> lock(data_ptr->mutex_);
  if (data_ptr->is_shutdown_) {
    return;
  }
  std::string str(z_string_data(z_loan(keystr)), z_string_len(z_loan(keystr)));
  switch (z_sample_kind(sample)) {
    case z_sample_kind_t::Z_SAMPLE_KIND_PUT:
      data_ptr->graph_cache_->parse_put(str);
      break;
    case z_sample_kind_t::Z_SAMPLE_KIND_DELETE:
      data_ptr->graph_cache_->parse_del(str);
      break;
    default:
      return;
  }

  // Trigger the ROS graph guard condition.
  rmw_ret_t rmw_ret = rmw_trigger_guard_condition(data_ptr->graph_guard_condition_.get());
  if (RMW_RET_OK != rmw_ret) {
    RMW_ZENOH_LOG_WARN_NAMED(
      "rmw_zenoh_cpp",
      "[graph_sub_data_handler] Unable to trigger graph guard condition."
    );
  }
}

///=============================================================================
rmw_context_impl_s::Data::Data(
  const std::string & enclave,
  z_owned_session_t session,
  std::optional<z_owned_shm_provider_t> shm_provider,
  const std::string & liveliness_str,
  std::shared_ptr<rmw_zenoh_cpp::GraphCache> graph_cache)
: enclave_(std::move(enclave)),
  session_(std::move(session)),
  shm_provider_(std::move(shm_provider)),
  liveliness_str_(std::move(liveliness_str)),
  graph_cache_(std::move(graph_cache)),
  is_shutdown_(false),
  next_entity_id_(0),
  is_initialized_(false)
{
  graph_guard_condition_ = std::make_unique<rmw_guard_condition_t>();
  graph_guard_condition_->implementation_identifier = rmw_zenoh_cpp::rmw_zenoh_identifier;
  graph_guard_condition_->data = &guard_condition_data_;
}

///=============================================================================
rmw_ret_t rmw_context_impl_s::Data::subscribe_to_ros_graph()
{
  std::lock_guard<std::mutex> lock(mutex_);
  if (is_initialized_) {
    return RMW_RET_OK;
  }
  // Setup the liveliness subscriber to receives updates from the ROS graph
  // and update the graph cache.
  // TODO(Yadunund): This closure is still not 100% thread safe as we are
  // passing Data* as the type erased argument to z_closure. Thus during
  // the execution of graph_sub_data_handler, the rawptr may be freed/reset
  // by a different thread. When we switch to zenoh-cpp we can replace z_closure
  // with a lambda that captures a weak_ptr<Data> by copy. The lambda and caputed
  // weak_ptr<Data> will have the same lifetime as the subscriber. Then within
  // graph_sub_data_handler, we would first lock to weak_ptr to check if the
  // shared_ptr<Data> exits. If it does, then even if a different thread calls
  // rmw_context_fini() to destroy rmw_context_impl_s, the locked
  // shared_ptr<Data> would live on until the graph_sub_data_handler callback.
  zc_liveliness_subscriber_options_t sub_options;
  zc_liveliness_subscriber_options_default(&sub_options);
  // Enable history option to get the old graph information before this session was started.
  sub_options.history = true;
  z_owned_closure_sample_t callback;
  z_closure(&callback, graph_sub_data_handler, nullptr, this);
  z_view_keyexpr_t keyexpr;
  z_view_keyexpr_from_str(&keyexpr, liveliness_str_.c_str());
  auto undeclare_z_sub = rcpputils::make_scope_exit(
    [this]() {
      z_undeclare_subscriber(z_move(this->graph_subscriber_));
    });
  if (zc_liveliness_declare_subscriber(
      &graph_subscriber_,
      z_loan(session_), z_loan(keyexpr),
      z_move(callback), &sub_options) != Z_OK)
  {
    RMW_SET_ERROR_MSG("unable to create zenoh subscription");
    return RMW_RET_ERROR;
  }

  undeclare_z_sub.cancel();
  is_initialized_ = true;
  return RMW_RET_OK;
}

///=============================================================================
rmw_ret_t rmw_context_impl_s::Data::shutdown()
{
  std::lock_guard<std::mutex> lock(mutex_);
  if (is_shutdown_) {
    return RMW_RET_OK;
  }

  z_undeclare_subscriber(z_move(graph_subscriber_));
  if (shm_provider_.has_value()) {
    z_drop(z_move(shm_provider_.value()));
  }
  // Close the zenoh session
  if (z_close(z_move(session_), NULL) != Z_OK) {
    RMW_SET_ERROR_MSG("Error while closing zenoh session");
    return RMW_RET_ERROR;
  }
  is_shutdown_ = true;
  return RMW_RET_OK;
}

///=============================================================================
rmw_context_impl_s::Data::~Data()
{
  auto ret = this->shutdown();
  static_cast<void>(ret);
}

///=============================================================================
rmw_context_impl_s::rmw_context_impl_s(
  const std::size_t domain_id,
  const std::string & enclave)
{
  // Initialize the zenoh configuration.
  z_owned_config_t config;
  rmw_ret_t ret;
  if ((ret =
    rmw_zenoh_cpp::get_z_config(
      rmw_zenoh_cpp::ConfigurableEntity::Session,
      &config)) != RMW_RET_OK)
  {
    throw std::runtime_error("Error configuring Zenoh session.");
  }

  // Check if shm is enabled.
  z_owned_string_t shm_enabled;
  zc_config_get_from_str(z_loan(config), Z_CONFIG_SHARED_MEMORY_KEY, &shm_enabled);
  auto always_free_shm_enabled = rcpputils::make_scope_exit(
    [&shm_enabled]() {
      z_drop(z_move(shm_enabled));
    });

  // Initialize the zenoh session.
  z_owned_session_t session;
  if (z_open(&session, z_move(config), NULL) != Z_OK) {
    RMW_SET_ERROR_MSG("Error setting up zenoh session");
    throw std::runtime_error("Error setting up zenoh session.");
  }
  auto close_session = rcpputils::make_scope_exit(
    [&session]() {
      z_close(z_move(session), NULL);
    });

  // TODO(Yadunund) Move this check into a separate thread.
  // Verify if the zenoh router is running if configured.
  const std::optional<uint64_t> configured_connection_attempts =
    rmw_zenoh_cpp::zenoh_router_check_attempts();
  if (configured_connection_attempts.has_value()) {
    ret = RMW_RET_ERROR;
    uint64_t connection_attempts = 0;
    // Retry until the connection is successful.
    while (ret != RMW_RET_OK && connection_attempts < configured_connection_attempts.value()) {
      if ((ret = rmw_zenoh_cpp::zenoh_router_check(z_loan(session))) != RMW_RET_OK) {
        ++connection_attempts;
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    if (ret != RMW_RET_OK) {
      throw std::runtime_error(
              "Unable to connect to a Zenoh router after " +
              std::to_string(configured_connection_attempts.value()) +
              " retries.");
    }
  }

  // Initialize the graph cache.
  const z_id_t zid = z_info_zid(z_loan(session));
  auto graph_cache = std::make_shared<rmw_zenoh_cpp::GraphCache>(zid);
  // Setup liveliness subscriptions for discovery.
  std::string liveliness_str = rmw_zenoh_cpp::liveliness::subscription_token(
    domain_id);

  // Initialize the shm manager if shared_memory is enabled in the config.
  std::optional<z_owned_shm_provider_t> shm_provider = std::nullopt;
  if (strncmp(z_string_data(z_loan(shm_enabled)), "true", z_string_len(z_loan(shm_enabled))) == 0) {
    RMW_ZENOH_LOG_DEBUG_NAMED("rmw_zenoh_cpp", "SHM is enabled");

    // TODO(yuyuan): determine the default alignment of SHM
    z_alloc_alignment_t alignment = {5};
    z_owned_memory_layout_t layout;
    z_memory_layout_new(&layout, SHM_BUFFER_SIZE_MB * 1024 * 1024, alignment);

    z_owned_shm_provider_t provider;
    if (z_posix_shm_provider_new(&provider, z_loan(layout)) != Z_OK) {
      RMW_ZENOH_LOG_ERROR_NAMED("rmw_zenoh_cpp", "Unable to create a SHM provider.");
      throw std::runtime_error("Unable to create shm manager.");
    }
    shm_provider = provider;
  }
  auto free_shm_provider = rcpputils::make_scope_exit(
    [&shm_provider]() {
      if (shm_provider.has_value()) {
        z_drop(z_move(shm_provider.value()));
      }
    });

  close_session.cancel();
  free_shm_provider.cancel();

  data_ = std::make_shared<Data>(
    std::move(enclave),
    std::move(session),
    std::move(shm_provider),
    std::move(liveliness_str),
    std::move(graph_cache));

  ret = data_->subscribe_to_ros_graph();
  if (ret != RMW_RET_OK) {
    throw std::runtime_error("Unable to subscribe to ROS Graph updates.");
  }
}

///=============================================================================
std::string rmw_context_impl_s::enclave() const
{
  std::lock_guard<std::mutex> lock(data_->mutex_);
  return data_->enclave_;
}

///=============================================================================
const z_loaned_session_t * rmw_context_impl_s::session() const
{
  std::lock_guard<std::mutex> lock(data_->mutex_);
  return z_loan(data_->session_);
}

///=============================================================================
std::optional<z_owned_shm_provider_t> & rmw_context_impl_s::shm_provider()
{
  std::lock_guard<std::mutex> lock(data_->mutex_);
  return data_->shm_provider_;
}

///=============================================================================
rmw_guard_condition_t * rmw_context_impl_s::graph_guard_condition()
{
  std::lock_guard<std::mutex> lock(data_->mutex_);
  return data_->graph_guard_condition_.get();
}

///=============================================================================
size_t rmw_context_impl_s::get_next_entity_id()
{
  std::lock_guard<std::mutex> lock(data_->mutex_);
  return data_->next_entity_id_++;
}

///=============================================================================
rmw_ret_t rmw_context_impl_s::shutdown()
{
  return data_->shutdown();
}

///=============================================================================
bool rmw_context_impl_s::is_shutdown() const
{
  std::lock_guard<std::mutex> lock(data_->mutex_);
  return data_->is_shutdown_;
}

///=============================================================================
std::shared_ptr<rmw_zenoh_cpp::GraphCache> rmw_context_impl_s::graph_cache()
{
  std::lock_guard<std::mutex> lock(data_->mutex_);
  return data_->graph_cache_;
}
