// DynSoA Runtime SDK

#pragma once
#if defined(_WIN32) || defined(_WIN64)
#  if defined(DYNSOA_BUILD_DLL)
#    define DYNSOA_API __declspec(dllexport)
#  else
#    define DYNSOA_API __declspec(dllimport)
#  endif
#else
#  define DYNSOA_API __attribute__((visibility("default")))
#endif
