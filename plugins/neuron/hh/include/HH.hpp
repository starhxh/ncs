#include <ncs/sim/Constants.h>

template<ncs::sim::DeviceType::Type MType>
VoltageGatedChannelSimulator<MType>::VoltageGatedChannelSimulator() {
  particle_indices_ = nullptr;
  particle_products_ = nullptr;
}

template<ncs::sim::DeviceType::Type MType>
VoltageGatedChannelSimulator<MType>::~VoltageGatedChannelSimulator() {
  if (particle_indices_) {
    ncs::sim::Memory<MType>::free(particle_indices_);
  }
  if (particle_products_) {
    ncs::sim::Memory<MType>::free(particle_products_);
  }
}

template<ncs::sim::DeviceType::Type MType>
bool VoltageGatedChannelSimulator<MType>::
update(ChannelUpdateParameters* parameters) {
  std::cout << "STUB: VoltageGatedChannelSimulator<MType>::update()" <<
    std::endl;
  return true;
}

template<ncs::sim::DeviceType::Type MType>
bool VoltageGatedChannelSimulator<MType>::init_() {
  num_particles_ = 0;
  for (auto i : this->instantiators_) {
    auto instantiator = (VoltageGatedInstantiator*)i;
    num_particles_ += instantiator->particles.size();
  }
  const auto CPU = ncs::sim::DeviceType::CPU;
  ParticleConstants<CPU> alpha;
  ParticleConstants<CPU> beta;
  if (!alpha.init(num_particles_) ||
      !beta.init(num_particles_)) {
    std::cerr << "Failed to allocate CPU particle buffer." << std::endl;
    return false;
  }
  if (!alpha_.init(num_particles_) || !beta_.init(num_particles_)) {
    std::cerr << "Failed to allocate MType particle buffer." << std::endl;
    return false;
  }
  bool result = true;
  result &= ncs::sim::Memory<MType>::malloc(conductance_, this->num_channels_);
  result &= ncs::sim::Memory<MType>::malloc(reversal_potential_, 
                                            this->num_channels_);
  result &= ncs::sim::Memory<MType>::malloc(particle_products_, 
                                            this->num_channels_);
  result &= ncs::sim::Memory<MType>::malloc(x_, num_particles_);
  result &= ncs::sim::Memory<MType>::malloc(power_, num_particles_);
  result &= ncs::sim::Memory<MType>::malloc(particle_indices_, num_particles_);
  result &= ncs::sim::Memory<MType>::malloc(neuron_id_by_particle_,
                                            num_particles_);
  if (!result) {
    std::cerr << "Failed to allocate MType memory." << std::endl;
    return false;
  }
  auto generate_particle = [=](ParticleConstants<CPU>& p,
                               void* instantiator,
                               ncs::spec::RNG* rng,
                               size_t index) {
    auto pci = (ParticleConstantsInstantiator*)instantiator;
    p.a[index] = pci->a->generateDouble(rng);
    p.b[index] = pci->b->generateDouble(rng);
    p.c[index] = pci->c->generateDouble(rng);
    p.d[index] = pci->d->generateDouble(rng);
    p.f[index] = pci->f->generateDouble(rng);
    p.h[index] = pci->h->generateDouble(rng);
  };
  float* conductance = new float[this->num_channels_];
  float* reversal_potential = new float[this->num_channels_];
  float* x = new float[num_particles_];
  float* power = new float[num_particles_];
  unsigned int* particle_indices = new unsigned int[num_particles_];
  unsigned int* neuron_id_by_particle = new unsigned int[num_particles_];
  for (size_t i = 0, particle_index = 0; i < this->num_channels_; ++i) {
    auto ci = (VoltageGatedInstantiator*)(this->instantiators_[i]);
    auto seed = this->seeds_[i];
    ncs::spec::RNG rng(seed);
    conductance[i] = ci->conductance->generateDouble(&rng);
    reversal_potential[i] = ci->reversal_potential->generateDouble(&rng);
    for (auto particle_instantiator : ci->particles) {
      auto vpi = (VoltageParticleInstantiator*)particle_instantiator;
      power[particle_index] = vpi->power->generateDouble(&rng);
      x[particle_index] = vpi->x_initial->generateDouble(&rng);
      generate_particle(alpha, vpi->alpha, &rng, particle_index);
      generate_particle(beta, vpi->beta, &rng, particle_index);
      particle_indices[particle_index] = i;
      neuron_id_by_particle[particle_index] = this->cpu_neuron_plugin_ids_[i];
      ++particle_index;
    }
  }

  using ncs::sim::mem::copy;
  result &= copy<MType, CPU>(conductance_, conductance, this->num_channels_);
  result &= copy<MType, CPU>(reversal_potential_, 
                             reversal_potential, 
                             this->num_channels_);
  result &= copy<MType, CPU>(x_, x, num_particles_);
  result &= copy<MType, CPU>(power_, power, num_particles_);
  result &= copy<MType, CPU>(particle_indices_,
                             particle_indices, 
                             num_particles_);
  result &= copy<MType, CPU>(neuron_id_by_particle_,
                             neuron_id_by_particle,
                             num_particles_);
  result &= alpha_.copyFrom(&alpha, num_particles_);
  result &= beta_.copyFrom(&beta, num_particles_);
  delete [] conductance;
  delete [] x;
  delete [] power;
  delete [] particle_indices;
  delete [] neuron_id_by_particle;
  if (!result) {
    std::cerr << "Failed to transfer data to device." << std::endl;
    return false;
  }

  return true;
}

template<ncs::sim::DeviceType::Type MType>
ParticleConstants<MType>::ParticleConstants()
  : a(nullptr),
    b(nullptr),
    c(nullptr),
    d(nullptr),
    f(nullptr),
    h(nullptr) {
}

template<ncs::sim::DeviceType::Type MType>
bool ParticleConstants<MType>::init(size_t num_constants) {
  bool result = true;
  result &= ncs::sim::Memory<MType>::malloc(a, num_constants);
  result &= ncs::sim::Memory<MType>::malloc(b, num_constants);
  result &= ncs::sim::Memory<MType>::malloc(c, num_constants);
  result &= ncs::sim::Memory<MType>::malloc(d, num_constants);
  result &= ncs::sim::Memory<MType>::malloc(f, num_constants);
  result &= ncs::sim::Memory<MType>::malloc(h, num_constants);
  return result;
}

template<ncs::sim::DeviceType::Type MType>
template<ncs::sim::DeviceType::Type SType>
bool ParticleConstants<MType>::copyFrom(ParticleConstants<SType>* source,
                                        size_t num_constants) {
  bool result = true;
  result &= ncs::sim::mem::copy<MType, SType>(a, source->a, num_constants);
  result &= ncs::sim::mem::copy<MType, SType>(b, source->b, num_constants);
  result &= ncs::sim::mem::copy<MType, SType>(c, source->c, num_constants);
  result &= ncs::sim::mem::copy<MType, SType>(d, source->d, num_constants);
  result &= ncs::sim::mem::copy<MType, SType>(f, source->f, num_constants);
  result &= ncs::sim::mem::copy<MType, SType>(h, source->h, num_constants);
  return result;
}

template<ncs::sim::DeviceType::Type MType>
ParticleConstants<MType>::~ParticleConstants() {
  if (a) {
    ncs::sim::Memory<MType>::free(a);
  }
  if (b) {
    ncs::sim::Memory<MType>::free(b);
  }
  if (c) {
    ncs::sim::Memory<MType>::free(c);
  }
  if (d) {
    ncs::sim::Memory<MType>::free(d);
  }
  if (f) {
    ncs::sim::Memory<MType>::free(f);
  }
  if (h) {
    ncs::sim::Memory<MType>::free(h);
  }
}

template<ncs::sim::DeviceType::Type MType>
HHSimulator<MType>::HHSimulator() {
  channel_simulators_.push_back(new VoltageGatedChannelSimulator<MType>());
  threshold_ = nullptr;
  resting_potential_ = nullptr;
  capacitance_ = nullptr;
  channel_current_subscription_ = nullptr;
  channel_updater_ = new ChannelUpdater<MType>();
}

template<ncs::sim::DeviceType::Type MType>
bool HHSimulator<MType>::addNeuron(ncs::sim::Neuron* neuron) {
  neurons_.push_back(neuron);
  return true;
}

template<ncs::sim::DeviceType::Type MType>
bool HHSimulator<MType>::
initialize(const ncs::spec::SimulationParameters* simulation_parameters) {
  simulation_parameters_ = simulation_parameters;
  using ncs::sim::Memory;
  num_neurons_ = neurons_.size();
  bool result = true;
  result &= Memory<MType>::malloc(threshold_, num_neurons_);
  result &= Memory<MType>::malloc(resting_potential_, num_neurons_);
  result &= Memory<MType>::malloc(capacitance_, num_neurons_);
  if (!result) {
    std::cerr << "Failed to allocate memory." << std::endl;
    return false;
  }
  
  float* threshold = new float[num_neurons_];
  float* resting_potential = new float[num_neurons_];
  float* capacitance = new float[num_neurons_];
  for (size_t i = 0; i < num_neurons_; ++i) {
    NeuronInstantiator* ni = (NeuronInstantiator*)(neurons_[i]->instantiator);
    ncs::spec::RNG rng(neurons_[i]->seed);
    threshold[i] = ni->threshold->generateDouble(&rng);
    resting_potential[i] = ni->resting_potential->generateDouble(&rng);
    capacitance[i] = ni->capacitance->generateDouble(&rng);
    unsigned int plugin_index = neurons_[i]->id.plugin;
    for (auto channel : ni->channels) {
      auto channel_type = channel->type;
      channel_simulators_[channel_type]->addChannel(channel, plugin_index, rng());
    }
  }

  const auto CPU = ncs::sim::DeviceType::CPU;
  auto copy = [num_neurons_](float* src, float* dst) {
    return Memory<CPU>::To<MType>::copy(src, dst, num_neurons_);
  };
  result &= copy(threshold, threshold_);
  result &= copy(resting_potential, resting_potential_);
  result &= copy(capacitance, capacitance_);

  delete [] threshold;
  delete [] resting_potential;
  delete [] capacitance;

  if (!result) {
    std::cerr << "Failed to copy data." << std::endl;
    return false;
  }

  for (size_t i = 0; i < ncs::sim::Constants::num_buffers; ++i) {
    auto blank = new NeuronBuffer<MType>();
    if (!blank->init(num_neurons_)) {
      delete blank;
      std::cerr << "Failed to initialize NeuronBuffer." << std::endl;
      return false;
    }
    addBlank(blank);
  }

  for (auto simulator : channel_simulators_) {
    if (!simulator->initialize()) {
      std::cerr << "Failed to initialize channel simulator." << std::endl;
      return false;
    }
  }

  channel_current_subscription_ = channel_updater_->subscribe();
  if (!channel_updater_->init(channel_simulators_,
                              this,
                              simulation_parameters,
                              num_neurons_,
                              ncs::sim::Constants::num_buffers)) {
    std::cerr << "Failed to initialize ChannelUpdater." << std::endl;
    return false;
  }

  if (!channel_updater_->start()) {
    std::cerr << "Failed to start ChannelUpdater." << std::endl;
    return false;
  }

  state_subscription_ = this->subscribe();

  // Publish the initial state
  auto blank = this->getBlank();
  ncs::sim::mem::copy<MType, MType>(blank->getVoltage(),
                                    resting_potential_,
                                    num_neurons_);
  this->publish(blank);
  return true;
}

template<ncs::sim::DeviceType::Type MType>
bool HHSimulator<MType>::initializeVoltages(float* plugin_voltages) {
  return ncs::sim::mem::copy<MType, MType>(plugin_voltages, 
                                           resting_potential_, 
                                           num_neurons_);
}

template<ncs::sim::DeviceType::Type MType>
bool HHSimulator<MType>::
update(ncs::sim::NeuronUpdateParameters* parameters) {
  std::cout << "STUB: HHSimulator<MType>::update()" << std::endl;
  return true;
}

template<ncs::sim::DeviceType::Type MType>
HHSimulator<MType>::~HHSimulator() {
  if (state_subscription_) {
    delete state_subscription_;
  }
  if (channel_current_subscription_) {
    delete channel_current_subscription_;
  }
  auto if_delete = [](float* p) {
    if (p) {
      ncs::sim::Memory<MType>::free(p);
    }
  };
  if_delete(threshold_);
  if_delete(resting_potential_);
  if_delete(capacitance_);
}