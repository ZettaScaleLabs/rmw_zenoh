#ifndef RMW_ZENOH_CPP__VISIBILITY_CONTROL_H_
#define RMW_ZENOH_CPP__VISIBILITY_CONTROL_H_

// This logic was borrowed (then namespaced) from the examples on the gcc wiki:
//     https://gcc.gnu.org/wiki/Visibility

#if defined _WIN32 || defined __CYGWIN__
  #ifdef __GNUC__
    #define RMW_ZENOH_CPP_EXPORT __attribute__ ((dllexport))
    #define RMW_ZENOH_CPP_IMPORT __attribute__ ((dllimport))
  #else
    #define RMW_ZENOH_CPP_EXPORT __declspec(dllexport)
    #define RMW_ZENOH_CPP_IMPORT __declspec(dllimport)
  #endif
  #ifdef RMW_ZENOH_CPP_BUILDING_LIBRARY
    #define RMW_ZENOH_CPP_PUBLIC RMW_ZENOH_CPP_EXPORT
  #else
    #define RMW_ZENOH_CPP_PUBLIC RMW_ZENOH_CPP_IMPORT
  #endif
  #define RMW_ZENOH_CPP_PUBLIC_TYPE RMW_ZENOH_CPP_PUBLIC
  #define RMW_ZENOH_CPP_LOCAL
#else
  #define RMW_ZENOH_CPP_EXPORT __attribute__ ((visibility("default")))
  #define RMW_ZENOH_CPP_IMPORT
  #if __GNUC__ >= 4
    #define RMW_ZENOH_CPP_PUBLIC __attribute__ ((visibility("default")))
    #define RMW_ZENOH_CPP_LOCAL  __attribute__ ((visibility("hidden")))
  #else
    #define RMW_ZENOH_CPP_PUBLIC
    #define RMW_ZENOH_CPP_LOCAL
  #endif
  #define RMW_ZENOH_CPP_PUBLIC_TYPE
#endif

#endif  // RMW_ZENOH_CPP__VISIBILITY_CONTROL_H_
