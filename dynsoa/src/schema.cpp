// DynSoA Runtime SDK

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
