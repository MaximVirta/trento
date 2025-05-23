// TRENTO: Reduced Thickness Event-by-event Nuclear Topology
// Copyright 2015 Jonah E. Bernhard, J. Scott Moreland
// MIT License

#include "collider.h"

#include <cmath>
#include <string>
#include <vector>

#include <boost/program_options/variables_map.hpp>

#include "fwd_decl.h"
#include "nucleus.h"

#include <iostream>


namespace trento {

namespace {

// Helper functions for Collider ctor.

// Create one nucleus from the configuration.
NucleusPtr create_nucleus(const VarMap& var_map, std::size_t index) {
  const auto& species = var_map["projectile"]
                        .as<std::vector<std::string>>().at(index);
  const auto& nucleon_dmin = var_map["nucleon-min-dist"].as<double>();
  const auto& a0 = var_map["a0"].as<double>();
  const auto& gamma_mean = var_map["y-mean"].as<double>();
  const auto& gamma_std = var_map["y-std"].as<double>();
  const auto& beta2_mean = var_map["beta2-mean"].as<double>();
  const auto& beta2_std = var_map["beta2-std"].as<double>();
  const auto& beta3 = var_map["beta3"].as<double>();
  const auto& beta4 = var_map["beta4"].as<double>();

  std::normal_distribution<double> ndist_gamma(gamma_mean, gamma_std); 
  double gamma = ndist_gamma(random::engine);

  std::normal_distribution<double> ndist_beta2(beta2_mean, beta2_std); 
  double beta2 = ndist_beta2(random::engine);

  return Nucleus::create(species, nucleon_dmin, a0, beta2, beta3, beta4, gamma);
}

// Determine the maximum impact parameter.  If the configuration contains a
// non-negative value for bmax, use it; otherwise, fall back to the minimum-bias
// default.
double determine_bmax(const VarMap& var_map,
    const Nucleus& A, const Nucleus& B, const NucleonCommon& nc) {
  auto bmax = var_map["b-max"].as<double>();
  if (bmax < 0.)
    bmax = A.radius() + B.radius() + nc.max_impact();
  return bmax;
}

// Determine the asymmetry parameter (Collider::asymmetry_) for a pair of
// nuclei.  It's just rA/(rA+rB), falling back to 1/2 if both radii are zero
// (i.e. for proton-proton).
double determine_asym(const Nucleus& A, const Nucleus& B) {
  double rA = A.radius();
  double rB = B.radius();
  double sum = rA + rB;
  if (sum < 0.1)
    return 0.5;
  else
    return rA/sum;
}

}  // unnamed namespace

// Lots of members to initialize...
// Several helper functions are defined above.
Collider::Collider(const VarMap& var_map)
    : nucleusA_(create_nucleus(var_map, 0)),
      nucleusB_(create_nucleus(var_map, 1)),
      nucleon_common_(var_map),
      nevents_(var_map["number-events"].as<int>()),
      calc_ncoll_(var_map["ncoll"].as<bool>()),
      calc_toColl_(var_map["toColl"].as<bool>()),
      bmin_(var_map["b-min"].as<double>()),
      bmax_(determine_bmax(var_map, *nucleusA_, *nucleusB_, nucleon_common_)),
      asymmetry_(determine_asym(*nucleusA_, *nucleusB_)),
      event_(var_map),
      output_(var_map) {
  // Constructor body begins here.
  // Set random seed if requested.
  auto seed = var_map["random-seed"].as<int64_t>();
  if (seed > 0)
    random::engine.seed(static_cast<random::Engine::result_type>(seed));
}

// See header for explanation.
Collider::~Collider() = default;

void Collider::run_events() {
  // The main event loop.
  for (int n = 0; n < nevents_; ++n) {
    // Sampling the impact parameter also implicitly prepares the nuclei for
    // event computation, i.e. by sampling nucleon positions and participants.
    auto collision_attr = sample_collision();
    double b = std::get<0>(collision_attr);
    int ncoll = std::get<1>(collision_attr);
    int nToCollide = std::get<2>(collision_attr);

    // Pass the prepared nuclei to the Event.  It computes the entropy profile
    // (thickness grid) and other event observables.
    event_.compute(*nucleusA_, *nucleusB_, nucleon_common_);

    // Write event data.
    output_(n, b, ncoll, nToCollide, event_);
  }
}

std::tuple<double, int, int> Collider::sample_collision() {
  // Sample impact parameters until at least one nucleon-nucleon pair
  // participates.  The bool 'collision' keeps track -- it is effectively a
  // logical OR over all possible participant pairs.
  // Returns the sampled impact parameter b, and binary collision number ncoll.
  double b;
  int ncoll = 0;
  bool collision = false;
  int nToCollide = 0;

  do {
    // Sample b from P(b)db = 2*pi*b.
    b = std::sqrt(bmin_ * bmin_ + (bmax_ * bmax_ - bmin_ * bmin_) * random::canonical<double>());

    // Offset each nucleus depending on the asymmetry parameter (see header).
    nucleusA_->sample_nucleons(asymmetry_ * b);
    nucleusB_->sample_nucleons((asymmetry_ - 1.) * b);

    // Check each nucleon-nucleon pair.
    for (auto&& A : *nucleusA_) {
      for (auto&& B : *nucleusB_) {
        auto new_collision = nucleon_common_.participate(A, B);
        if (new_collision && calc_ncoll_) ++ncoll;
        collision = new_collision || collision;
      }
    }
    if (calc_toColl_) nToCollide++;
  } while (!collision);

  return std::make_tuple(b, ncoll, nToCollide);
}

}  // namespace trento
