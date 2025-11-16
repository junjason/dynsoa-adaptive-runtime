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

#include "dynsoa/schema.h"
#include <unordered_map>

namespace dynsoa {
static std::unordered_map<std::string, Component> g_components;
static std::vector<ArchetypeDesc> g_archetypes;

void define_component(const Component& c) { g_components[c.name] = c; }

ArchetypeId define_archetype(const char* name, const char** comps, int count) {
  ArchetypeDesc desc;
  desc.name = name ? name : "";
  desc.components.reserve(count);
  for (int i=0;i<count;++i) desc.components.emplace_back(comps[i]);
  g_archetypes.push_back(desc);
  return static_cast<ArchetypeId>(g_archetypes.size()); // 1-based id
}

} // namespace dynsoa
