// DynSoA Runtime SDK

#pragma once
#include "types.h"

namespace dynsoa {

enum class LayoutKind : std::uint8_t { AoS=0, SoA=1, AoSoA=2, Matrix=3 };

struct RetilePlan {
  LayoutKind to = LayoutKind::SoA;
  int        tile_or_block = 0;
  double     est_cost_us = 0.0;
  double     est_gain_us = 0.0;
};

LayoutKind current_layout(ViewId v);

RetilePlan plan_aosoa(ViewId v, int tile);
RetilePlan plan_matrix(ViewId v, int block);

bool retile(ViewId v, const RetilePlan& plan);
bool retile_to_soa(ViewId v);

} // namespace dynsoa
