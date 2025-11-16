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
#include "layout.h"
#include <string>
#include <vector>

namespace dynsoa {

// Learnable coefficients (global)
struct LearnState {
  double a_div  = 0.06; // weights for div/mem/tail terms in gain model
  double a_mem  = 0.04;
  double a_tail = 0.02;
};

struct PolicyTrigger {
  std::string when;     // e.g., "branch_div > 0.2 && warp_eff < 0.8"
  std::string action;   // "RETILE_AOSOA" | "RETILE_SOA" | "PACK_MATRIX"
  int         arg = 0;  // tile or block
  double      priority = 1.0;
};
struct Policy {
  std::vector<PolicyTrigger> triggers;
  int min_frames_between_retiles = 5;
  int cooloff_frames = 10;
};

void scheduler_set_policy(const Policy& p);
void scheduler_on_begin_frame();
void scheduler_on_end_frame();

// Learning & persistence
LearnState scheduler_learn_for();
void       scheduler_set_persist_path(const char*);
void       scheduler_load_state();
void       scheduler_save_state();

} // namespace dynsoa
