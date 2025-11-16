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
#include "types.h"
#include <vector>
#include <string>

namespace dynsoa {

struct ArchetypeDesc {
  std::string name;
  std::vector<std::string> components;
};

void define_component(const Component& c);
ArchetypeId define_archetype(const char* name, const char** components, int count);

} // namespace dynsoa
