// DynSoA Runtime SDK

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
