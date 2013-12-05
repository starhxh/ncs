#include <ncs/sim/Bit.h>

namespace cuda {

void checkPrefire(const ncs::sim::Bit::Word* synaptic_fire,
                  const float* tau_facilitations,
                  const float* tau_depressions,
                  const float* max_conductances,
                  const float* tau_ltps,
                  const float* tau_ltds,
                  const float* A_ltp_minimums,
                  const float* last_postfire_times,
                  const float* tau_postsynaptic_conductances,
                  const float* reversal_potentials,
                  const unsigned int* device_neuron_device_ids,
                  const float* neuron_voltage,
                  float* utilizations,
                  float* redistributions,
                  float* base_utilizations,
                  float* last_prefire_times,
                  float* A_ltps,
                  float* A_ltds,
                  float* synaptic_current,
                  float simulation_time,
                  unsigned int* fire_indices,
                  float* fire_times,
                  float* psg_maxes,
                  unsigned int* total_firings,
                  unsigned int num_synapses);

void addOldFirings(const unsigned int* old_fire_indices,
                   const float* old_fire_times,
                   const float* psg_waveform_durations,
                   const float* old_psg_maxes,
                   const float* tau_postsynaptic_conductances,
                   const unsigned int* device_neuron_device_ids,
                   const float* reversal_potentials,
                   unsigned int* new_fire_indices,
                   float* new_fire_times,
                   float* new_psg_maxes,
                   unsigned int* total_firings,
                   float* synaptic_current,
                   const float* neuron_voltage,
                   float simulation_time,
                   unsigned int num_old_firings);

} // namespace cuda
