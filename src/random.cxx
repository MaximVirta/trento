// TRENTO: Reduced Thickness Event-by-event Nuclear Topology
// Copyright 2015 Jonah E. Bernhard, J. Scott Moreland
// MIT License

#include "random.h"


namespace trento { namespace random {

// Seed random number generator from hardware device.
Engine engine{std::random_device{}()};
std::poisson_distribution<int> poissondist{};

void setDistribution(double lambda) {
  poissondist = std::poisson_distribution<int>(lambda);
}

}}  // namespace trento::random
