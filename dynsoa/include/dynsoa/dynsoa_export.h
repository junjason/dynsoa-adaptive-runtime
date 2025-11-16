// DynSoA Runtime SDK
// (C) 2025 Sungmin "Jason" Jun
//
// Covered under:
//  - U.S. Patent Application No. 19/303,020
//  - U.S. Provisional No. 63/775,990
//  - CIP: Systems and Methods for Adaptive Optimization and Coordination of Data Layout and Execution
//
// Licensed under the Mozilla Public License 2.0 (MPL 2.0).
// Commercial use requires a separate commercial license.
// Unauthorized commercial use may infringe one or more patents.

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
