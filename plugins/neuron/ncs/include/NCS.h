#pragma once
#include <thread>

#include <ncs/sim/DataBuffer.h>
#include <ncs/sim/NeuronSimulator.h>
#include <ncs/spec/Generator.h>

enum ChannelType {
  VoltageGated = 0,
  CalciumGated = 1
};

struct ChannelInstantiator {
  ChannelType type;
};

struct ParticleConstantsInstantiator {
  ncs::spec::Generator* a;
  ncs::spec::Generator* b;
  ncs::spec::Generator* c;
  ncs::spec::Generator* d;
  ncs::spec::Generator* f;
  ncs::spec::Generator* h;
};

struct VoltageParticleInstantiator {
  ParticleConstantsInstantiator* alpha;
  ParticleConstantsInstantiator* beta;
  ncs::spec::Generator* power;
};

struct VoltageGatedInstantiator : public ChannelInstantiator {
  ncs::spec::Generator* conductance;
  std::vector<VoltageGatedInstantiator*> particles;
};

struct NeuronInstantiator {
  ncs::spec::Generator* threshold;
  ncs::spec::Generator* resting_potential;
  ncs::spec::Generator* calcium;
  ncs::spec::Generator* calcium_spike_increment;
  ncs::spec::Generator* tau_calcium;
  ncs::spec::Generator* leak_reversal_potential;
  ncs::spec::Generator* leak_conductance;
  ncs::spec::Generator* tau_membrane;
  ncs::spec::Generator* r_membrane;
  std::vector<ChannelInstantiator*> channels;
};

template<ncs::sim::DeviceType::Type MType>
class ChannelCurrentBuffer : public ncs::sim::DataBuffer {
public:
  ChannelCurrentBuffer();
  bool init(size_t num_neurons);
  float* getCurrent();
  ~ChannelCurrentBuffer();
private:
  float* current_;
};

template<ncs::sim::DeviceType::Type MType>
class NeuronBuffer : public ncs::sim::DataBuffer {
public:
  NeuronBuffer();
  bool init(size_t num_neurons);
  float* getVoltage();
  float* getCalcium();
  ~NeuronBuffer();
private:
  float* voltage_;
  float* calcium_;
};

template<ncs::sim::DeviceType::Type MType>
class ChannelSimulator {
public:
  ChannelSimulator();
  bool addChannel(void* instantiator,
                  unsigned int neuron_plugin_id,
                  int seed);
  bool initialize();
  virtual ~ChannelSimulator();
protected:
  virtual bool init_() = 0;
  std::vector<void*> instantiators_;
  std::vector<unsigned int> cpu_neuron_plugin_ids_;
  std::vector<int> seeds_;
  unsigned int* neuron_plugin_ids_;
  size_t num_channels_;
private:
};

template<ncs::sim::DeviceType::Type MType>
class VoltageGatedChannelSimulator : public ChannelSimulator<MType> {
public:
  VoltageGatedChannelSimulator();
  virtual ~VoltageGatedChannelSimulator();
protected:
  virtual bool init_();
private:
  struct ParticleConstants {
    ParticleConstants();
    ~ParticleConstants();
    float* a;
    float* b;
    float* c;
    float* d;
    float* f;
    float* h;
  };
  ParticleConstants alpha_;
  ParticleConstants beta_;
  unsigned int* particle_indices_;
  float* particle_products_;
};

template<ncs::sim::DeviceType::Type MType>
class ChannelUpdater 
  : public ncs::sim::SpecificPublisher<ChannelCurrentBuffer<MType>> {
public:
  ChannelUpdater();
  typedef ncs::sim::SpecificPublisher<NeuronBuffer<MType>> NeuronPublisher;
  bool init(std::vector<ChannelSimulator<MType>*> simulators,
            NeuronPublisher* source_publisher,
            size_t num_neurons,
            size_t num_buffers);
  bool start();
  ~ChannelUpdater();
private:
  std::thread thread_;
  typename NeuronPublisher::Subscription* neuron_subscription_;
};

template<ncs::sim::DeviceType::Type MType>
class NCSSimulator 
  : public ncs::sim::NeuronSimulator<MType>,
    public ncs::sim::SpecificPublisher<NeuronBuffer<MType>> {
public:
  NCSSimulator();
  virtual bool addNeuron(ncs::sim::Neuron* neuron);
  virtual bool initialize();
  virtual bool initializeVoltages(float* plugin_voltages);
  virtual bool update(ncs::sim::NeuronUpdateParameters* parameters);
  virtual ~NCSSimulator();
private:
  std::vector<ChannelSimulator<MType>*> channel_simulators_;
  std::vector<ncs::sim::Neuron*> neurons_;
  size_t num_neurons_;
  ChannelUpdater<MType>* channel_updater_;
  typename ChannelUpdater<MType>::Subscription* channel_current_subscription_;

  float* threshold_;
  float* resting_potential_;
  float* calcium_;
  float* calcium_spike_increment_;
  float* tau_calcium_;
  float* leak_reversal_potential_;
  float* leak_conductance_;
  float* tau_membrane_;
  float* r_membrane_;
};

#include "NCS.hpp"