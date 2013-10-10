#pragma once

#include <mutex>

#include <ncs/sim/Bit.h>

namespace ncs {

namespace sim {

struct SynapseUpdateParameters {
  const Bit::Word* synaptic_fire;
  const float* neuron_voltage;
  float* synaptic_current;
  std::mutex* write_lock;
};

} // namespace sim

} // namespace ncs