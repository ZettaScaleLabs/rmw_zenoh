cmake_minimum_required(VERSION 3.16)
project(rmw_zenoh_cpp)

# Default to C++17
if(NOT CMAKE_CXX_STANDARD)
  set(CMAKE_CXX_STANDARD 17)
  set(CMAKE_CXX_STANDARD_REQUIRED ON)
endif()

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# find dependencies
find_package(ament_cmake REQUIRED)

find_package(ament_index_cpp REQUIRED)
find_package(fastcdr CONFIG REQUIRED)
find_package(rcpputils REQUIRED)
find_package(rcutils REQUIRED)
find_package(rosidl_typesupport_fastrtps_c REQUIRED)
find_package(rosidl_typesupport_fastrtps_cpp REQUIRED)
find_package(rmw REQUIRED)
find_package(zenoh_c_vendor REQUIRED)

# include_directories(include)

add_library(${PROJECT_NAME} SHARED
  src/detail/attachment_helpers.cpp
  src/detail/cdr.cpp
  src/detail/event.cpp
  src/detail/identifier.cpp
  src/detail/graph_cache.cpp
  src/detail/guard_condition.cpp
  src/detail/liveliness_utils.cpp
  src/detail/logging.cpp
  src/detail/message_type_support.cpp
  src/detail/qos.cpp
  src/detail/rmw_context_impl_s.cpp
  src/detail/rmw_data_types.cpp
  src/detail/rmw_publisher_data.cpp
  src/detail/rmw_node_data.cpp
  src/detail/rmw_service_data.cpp
  src/detail/rmw_subscription_data.cpp
  src/detail/service_type_support.cpp
  src/detail/simplified_xxhash3.cpp
  src/detail/type_support.cpp
  src/detail/type_support_common.cpp
  src/detail/zenoh_config.cpp
  src/detail/zenoh_router_check.cpp
  src/detail/zenoh_utils.cpp
  src/rmw_event.cpp
  src/rmw_get_network_flow_endpoints.cpp
  src/rmw_get_node_info_and_types.cpp
  src/rmw_get_service_names_and_types.cpp
  src/rmw_get_topic_endpoint_info.cpp
  src/rmw_get_topic_names_and_types.cpp
  src/rmw_init_options.cpp
  src/rmw_init.cpp
  src/rmw_qos.cpp
  src/rmw_zenoh.cpp
)

target_include_directories(${PROJECT_NAME} PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include/${PROJECT_NAME}>)

target_link_libraries(rmw_zenoh_cpp
  PRIVATE
    ament_index_cpp::ament_index_cpp
    fastcdr
    rcpputils::rcpputils
    rcutils::rcutils
    rosidl_typesupport_fastrtps_c::rosidl_typesupport_fastrtps_c
    rosidl_typesupport_fastrtps_cpp::rosidl_typesupport_fastrtps_cpp
    rmw::rmw
)
target_link_libraries(rmw_zenoh_cpp
  PUBLIC
    zenohc::lib
)

configure_rmw_library(rmw_zenoh_cpp)

target_compile_definitions(rmw_zenoh_cpp
  PRIVATE
    RMW_VERSION_MAJOR=${rmw_VERSION_MAJOR}
    RMW_VERSION_MINOR=${rmw_VERSION_MINOR}
    RMW_VERSION_PATCH=${rmw_VERSION_PATCH}
)

register_rmw_implementation(
  "c:rosidl_typesupport_c:rosidl_typesupport_fastrtps_c:rosidl_typesupport_introspection_c"
  "cpp:rosidl_typesupport_cpp:rosidl_typesupport_fastrtps_cpp:rosidl_typesupport_introspection_cpp")

if(BUILD_TESTING)
  find_package(ament_cmake_copyright REQUIRED)
  find_package(ament_cmake_cpplint REQUIRED)
  find_package(ament_cmake_lint_cmake REQUIRED)
  find_package(ament_cmake_uncrustify REQUIRED)
  find_package(ament_cmake_xmllint REQUIRED)

  set(_linter_excludes src/detail/ordered_hash.hpp src/detail/ordered_map.hpp)

  ament_copyright(EXCLUDE src/detail/simplified_xxhash3.cpp src/detail/simplified_xxhash3.hpp)
  ament_cpplint(EXCLUDE ${_linter_excludes})
  ament_lint_cmake()
  ament_uncrustify(EXCLUDE ${_linter_excludes})
  ament_xmllint()
endif()

# install the include headers
install(
  DIRECTORY include/
  DESTINATION include/${PROJECT_NAME}
)

# install the targets
install(
  TARGETS rmw_zenoh_cpp
  EXPORT export_${PROJECT_NAME}
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin
)

install(
  DIRECTORY config
  DESTINATION share/${PROJECT_NAME}
)

add_executable(rmw_zenohd
  src/detail/liveliness_utils.cpp
  src/detail/logging.cpp
  src/detail/qos.cpp
  src/detail/simplified_xxhash3.cpp
  src/detail/zenoh_config.cpp
  src/zenohd/main.cpp
)

target_link_libraries(rmw_zenohd
  PRIVATE
    ament_index_cpp::ament_index_cpp
    rcutils::rcutils
    rcpputils::rcpputils
    rmw::rmw
    zenohc::lib
)

install(
  TARGETS rmw_zenohd
  DESTINATION lib/${PROJECT_NAME}
)

# export targets
ament_export_targets(export_${PROJECT_NAME} HAS_LIBRARY_TARGET)

# export zenohc dependency for ament
ament_export_dependencies(zenoh_c_vendor)

# Export old-style CMake variables
ament_export_include_directories("include/${PROJECT_NAME}")
ament_export_libraries(${PROJECT_NAME})

# Export modern CMake targets
ament_export_targets(export_${PROJECT_NAME})

ament_package()
