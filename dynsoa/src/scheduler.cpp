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

#include "dynsoa/scheduler.h"
#include "dynsoa/metrics.h"
#include "dynsoa/layout.h"
#include <unordered_map>
#include <algorithm>
#include <fstream>
#include <sstream>
#include <cstdlib>
#include <cmath>
#include <cstdio>

static bool g_verbose_init = false;
static bool g_verbose = false;
static std::ofstream g_learn_csv;

static void ensure_verbose_init() {
  if (g_verbose_init) return;
  g_verbose_init = true;
  if (const char* v = std::getenv("DYNSOA_VERBOSE")) g_verbose = (std::atoi(v) != 0);
  const char* path = std::getenv("DYNSOA_LEARN_LOG");
  if (path && *path) {
    g_learn_csv.open(path, std::ios::out | std::ios::trunc);
    if (g_learn_csv.is_open()) {
      g_learn_csv << "frame,view,phase,action,to,tile,cost_us,gain_est_us,score,base_us,post_us,realized_us,"
                     "a_div,a_mem,a_tail,a_div_new,a_mem_new,a_tail_new\n";
    }
  }
}
static void vprint(const std::string& s){
  if (!g_verbose) return;
  std::fprintf(stderr, "%s\n", s.c_str());
}


namespace dynsoa {

struct BanditStat {
  double mean = 0.0;
  double m2 = 0.0;
  int n = 0;
  void update(double r) {
    n++;
    double delta = r - mean;
    mean += delta / n;
    m2 += delta * (r - mean);
  }
  double var() const { return (n>1? m2 / (n-1) : 0.0); }
};

// For each view, we keep stats per action key
// action key encoding: to*100000 + tile_or_block
static std::unordered_map<ViewId, std::unordered_map<long long, BanditStat>> g_bandit;
static int g_bandit_t = 0;

// candidate actions we consider each decision epoch
static std::vector<RetilePlan> catalog_actions(ViewId v) {
  std::vector<RetilePlan> c;
  c.push_back(plan_aosoa(v, 64));
  c.push_back(plan_aosoa(v, 128));
  c.push_back(plan_aosoa(v, 256));
  c.push_back(plan_matrix(v, 64));
  return c;
}

// Choose via UCB1 on reward (realized_us - est_cost_us), with small epsilon exploration.
// We use baseline p95/mean as context implicitly through reward; cheap but effective.
static RetilePlan pick_with_ucb(ViewId v, const std::vector<RetilePlan>& C) {
  g_bandit_t++;
  double eps = 0.05; // exploration
  if ((double)std::rand() / RAND_MAX < eps) return C[std::rand() % C.size()];

  auto& mp = g_bandit[v];
  double best = -1e100;
  RetilePlan bestp = C[0];
  for (const auto& p : C) {
    long long key = (long long)p.to * 100000LL + p.tile_or_block;
    auto it = mp.find(key);
    double mean = 0.0; int n = 0;
    if (it != mp.end()) { mean = it->second.mean; n = it->second.n; }
    double bonus = (n>0 ? std::sqrt(2.0 * std::log(std::max(2,g_bandit_t)) / n) : 1.0);
    double ucb = mean + bonus;
    if (ucb > best) { best = ucb; bestp = p; }
  }
  return bestp;
}

// Update bandit stat after learning step
static void bandit_update(ViewId v, const RetilePlan& p, double realized_us) {
  long long key = (long long)p.to * 100000LL + p.tile_or_block;
  double reward = realized_us - p.est_cost_us; // net improvement
  g_bandit[v][key].update(reward);
}


static Policy g_policy;
static std::unordered_map<ViewId,int> g_cooldown;
static int g_frame_idx = 0;

static LearnState g_learn;                    // global coefficients
static std::string g_persist_path = "dynsoa_learn.json";

static std::unordered_map<ViewId,double> g_pre_action_baseline;
static std::unordered_map<ViewId,int>    g_action_frame;

static double field_value(const std::string& name, const FrameAgg& a) {
  if (name == "mean_us")       return a.mean_us;
  if (name == "p95_us")        return a.p95_us;
  if (name == "p99_us")        return a.p99_us;
  if (name == "warp_eff")      return a.warp_eff;
  if (name == "branch_div")    return a.branch_div;
  if (name == "mem_coalesce")  return a.mem_coalesce;
  if (name == "l2_miss")       return a.l2_miss;
  if (name == "tail_ratio")    return a.tail_ratio;
  return 0.0;
}
static void trim(std::string& s){
  auto l = s.find_first_not_of(" \t");
  auto r = s.find_last_not_of(" \t");
  if (l==std::string::npos) { s.clear(); return; }
  s = s.substr(l, r-l+1);
}
static bool eval_atom(std::string expr, const FrameAgg& a) {
  trim(expr);
  const char* ops[] = {">=", "<=", "==", ">", "<"};
  std::string op; size_t pos = std::string::npos;
  for (auto* o : ops) { pos = expr.find(o); if (pos != std::string::npos) { op = o; break; } }
  if (pos == std::string::npos) return false;
  std::string lhs = expr.substr(0, pos); trim(lhs);
  std::string rhs = expr.substr(pos + op.size()); trim(rhs);
  double L = field_value(lhs, a);
  double R = std::stod(rhs);
  if (op == ">")  return L >  R;
  if (op == "<")  return L <  R;
  if (op == ">=") return L >= R;
  if (op == "<=") return L <= R;
  if (op == "==") return std::fabs(L - R) < 1e-9;
  return false;
}
static bool eval_pred(const std::string& when, const FrameAgg& a) {
  auto andpos = when.find("&&");
  if (andpos != std::string::npos) {
    auto left = when.substr(0, andpos);
    auto right= when.substr(andpos+2);
    return eval_atom(left, a) && eval_atom(right, a);
  }
  auto orpos = when.find("||");
  if (orpos != std::string::npos) {
    auto left = when.substr(0, orpos);
    auto right= when.substr(orpos+2);
    return eval_atom(left, a) || eval_atom(right, a);
  }
  return eval_atom(when, a);
}

void scheduler_set_policy(const Policy& p) { g_policy = p; }
void scheduler_on_begin_frame() { ++g_frame_idx; }

void scheduler_on_end_frame() {
  ensure_verbose_init();
struct Cand { ViewId v; RetilePlan plan; double score; };
  std::vector<Cand> C;

  for (ViewId v=1; v<=64; ++v) {
    auto agg = aggregate(v, 3);
    if (agg.mean_us == 0 && agg.p95_us == 0) continue;
    if (g_cooldown[v] > 0) { --g_cooldown[v]; continue; }

    for (auto& t : g_policy.triggers) {
      if (!eval_pred(t.when, agg)) continue;
      RetilePlan p;
      if (t.action == "RETILE_AOSOA") p = plan_aosoa(v, t.arg);
      else if (t.action == "RETILE_SOA") { p.to = LayoutKind::SoA; }
      else if (t.action == "PACK_MATRIX") p = plan_matrix(v, t.arg);

      double score = t.priority * (p.est_gain_us / std::max(1.0, p.est_cost_us));
      if (score > 0.05) C.push_back({v, p, score});
    }
  }

  std::sort(C.begin(), C.end(), [](auto& a, auto& b){
    if (a.score != b.score) return a.score > b.score;
    return a.v < b.v;
  });

  int budget_us = 200000; // could be piped from Config
  int used = 0;

  for (auto& c : C) {
    if (used + (int)c.plan.est_cost_us <= budget_us) {
      FrameAgg before = aggregate(c.v, 3);
      double baseline = (before.p95_us>0 ? before.p95_us : (before.mean_us>0?before.mean_us:0.0));
      if (baseline > 0) g_pre_action_baseline[c.v] = baseline;

      if (c.plan.to == LayoutKind::SoA) retile_to_soa(c.v);
      else                               retile(c.v, c.plan);

      used += (int)c.plan.est_cost_us;
      g_cooldown[c.v] = g_policy.cooloff_frames;
      g_action_frame[c.v] = g_frame_idx;
      if (g_verbose) {
        char buf[512];
        std::snprintf(buf, sizeof(buf),
          "%d,%llu,apply,%s,%d,%d,%.3f,%.3f,%.3f,%.3f,NA,NA,%.5f,%.5f,%.5f,NA,NA,NA",
          g_frame_idx, (unsigned long long)c.v,
          (c.plan.to == LayoutKind::AoSoA ? "RETILE_AOSOA" :
           c.plan.to == LayoutKind::SoA   ? "RETILE_SOA"   :
           c.plan.to == LayoutKind::Matrix? "PACK_MATRIX"  : "UNKNOWN"),
          (int)c.plan.to, c.plan.tile_or_block,
          c.plan.est_cost_us, c.plan.est_gain_us, c.score, baseline,
          g_learn.a_div, g_learn.a_mem, g_learn.a_tail);
        if (g_learn_csv.is_open()) g_learn_csv << buf << "\n";
        vprint(std::string("scheduler: applied action: ") + buf);
      }
    }
  }

  // Online learning: update 2+ frames after action
  for (auto it = g_action_frame.begin(); it != g_action_frame.end(); ++it) {
    ViewId v = it->first;
    int act_frame = it->second;
    if (g_frame_idx - act_frame < 2) continue;

    auto base_it = g_pre_action_baseline.find(v);
    if (base_it == g_pre_action_baseline.end()) continue;
    double base = base_it->second;

    FrameAgg after = aggregate(v, 3);
    double obs = (after.p95_us>0 ? after.p95_us : (after.mean_us>0?after.mean_us:base));
    if (obs <= 0 || base <= 0) continue;

    double realized_gain = std::max(0.0, base - obs);

    double div_term  = std::max(0.0, after.branch_div - 0.15);
    double mem_term  = std::max(0.0, 0.75 - after.mem_coalesce);
    double tail_term = std::max(0.0, after.tail_ratio - 1.10);
    double denom = 1e-6 + (div_term*div_term + mem_term*mem_term + tail_term*tail_term);

    double pred = base * (g_learn.a_div*div_term + g_learn.a_mem*mem_term + g_learn.a_tail*tail_term);
    double err  = realized_gain - pred;
    double lr   = 0.10;

    g_learn.a_div  = std::max(0.0, std::min(0.25, g_learn.a_div  + lr * (err/base) * (div_term / denom)));
    g_learn.a_mem  = std::max(0.0, std::min(0.25, g_learn.a_mem  + lr * (err/base) * (mem_term / denom)));
    g_learn.a_tail = std::max(0.0, std::min(0.25, g_learn.a_tail + lr * (err/base) * (tail_term/ denom)));

    if (g_verbose) {
      char buf[512];
      std::snprintf(buf, sizeof(buf),
        "%d,%llu,learn,NA,NA,NA,NA,NA,NA,%.3f,%.3f,%.3f,%.5f,%.5f,%.5f,%.5f,%.5f,%.5f",
        g_frame_idx, (unsigned long long)v,
        base, obs, realized_gain,
        g_learn.a_div, g_learn.a_mem, g_learn.a_tail,
        g_learn.a_div, g_learn.a_mem, g_learn.a_tail);
      if (g_learn_csv.is_open()) g_learn_csv << buf << "\n";
      vprint(std::string("scheduler: learned: ") + buf);
    }
    g_pre_action_baseline.erase(v);
}
}

LearnState scheduler_learn_for() { return g_learn; }

void scheduler_set_persist_path(const char* p) {
  if (p && *p) g_persist_path = p;
}

void scheduler_load_state() {
  if (const char* envp = std::getenv("DYNSOA_LEARN_PATH")) g_persist_path = envp;
  std::ifstream fin(g_persist_path);
  if (!fin) return;
  std::stringstream ss; ss << fin.rdbuf();
  std::string s = ss.str();
  auto findNum = [&](const char* key)->double{
    auto pos = s.find(key);
    if (pos == std::string::npos) return NAN;
    pos = s.find(':', pos);
    if (pos == std::string::npos) return NAN;
    size_t end = s.find_first_of(",}", pos+1);
    return std::atof(s.substr(pos+1, end-pos-1).c_str());
  };
  double d = findNum("\"a_div\"");  if (!std::isnan(d)) g_learn.a_div = d;
  double m = findNum("\"a_mem\"");  if (!std::isnan(m)) g_learn.a_mem = m;
  double t = findNum("\"a_tail\""); if (!std::isnan(t)) g_learn.a_tail= t;
}

void scheduler_save_state() {
  std::ofstream fout(g_persist_path, std::ios::out | std::ios::trunc);
  if (!fout) return;
  fout << "{\n";
  fout << "  \"a_div\": "  << g_learn.a_div  << ",\n";
  fout << "  \"a_mem\": "  << g_learn.a_mem  << ",\n";
  fout << "  \"a_tail\": " << g_learn.a_tail << "\n";
  fout << "}\n";
}

} // namespace dynsoa
