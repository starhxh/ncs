#include <ncs/sim/Constants.h>

template<ncs::sim::DeviceType::Type MType>
ChannelCurrentBuffer<MType>::ChannelCurrentBuffer() 
  : current_(nullptr) {
}

template<ncs::sim::DeviceType::Type MType>
bool ChannelCurrentBuffer<MType>::init(size_t num_neurons) {
  num_neurons_ = num_neurons;
  if (num_neurons_ > 0) {
    return ncs::sim::Memory<MType>::malloc(current_, num_neurons_);
  }
  return true;
}

template<ncs::sim::DeviceType::Type MType>
void ChannelCurrentBuffer<MType>::clear() {
  ncs::sim::Memory<MType>::zero(current_, num_neurons_);
}

template<ncs::sim::DeviceType::Type MType>
float* ChannelCurrentBuffer<MType>::getCurrent() {
  return current_;
}

template<ncs::sim::DeviceType::Type MType>
ChannelCurrentBuffer<MType>::~ChannelCurrentBuffer() {
  if (current_) {
    ncs::sim::Memory<MType>::free(current_);
  }
}

template<ncs::sim::DeviceType::Type MType>
NeuronBuffer<MType>::NeuronBuffer()
  : voltage_(nullptr),
    calcium_(nullptr) {
}

template<ncs::sim::DeviceType::Type MType>
bool NeuronBuffer<MType>::init(size_t num_neurons) {
  bool result = true;
  result &= ncs::sim::Memory<MType>::malloc(voltage_, num_neurons);
  result &= ncs::sim::Memory<MType>::malloc(calcium_, num_neurons);
  return true;
}

template<ncs::sim::DeviceType::Type MType>
float* NeuronBuffer<MType>::getVoltage() {
  return voltage_;
}

template<ncs::sim::DeviceType::Type MType>
float* NeuronBuffer<MType>::getCalcium() {
  return calcium_;
}

template<ncs::sim::DeviceType::Type MType>
NeuronBuffer<MType>::~NeuronBuffer() {
  if (voltage_) {
    ncs::sim::Memory<MType>::free(voltage_);
  }
  if (calcium_) {
    ncs::sim::Memory<MType>::free(calcium_);
  }
}

template<ncs::sim::DeviceType::Type MType>
ChannelSimulator<MType>::ChannelSimulator() 
  : neuron_plugin_ids_(nullptr) {
}

template<ncs::sim::DeviceType::Type MType>
bool ChannelSimulator<MType>::initialize() {
  num_channels_ = cpu_neuron_plugin_ids_.size();
  if (!ncs::sim::Memory<MType>::malloc(neuron_plugin_ids_, num_channels_)) {
    std::cerr << "Failed to allocate memory." << std::endl;
    return false;
  }
  const auto CPU = ncs::sim::DeviceType::CPU;
  using namespace ncs::sim;
  if (!ncs::sim::mem::copy<MType, CPU>(neuron_plugin_ids_,
                                       cpu_neuron_plugin_ids_.data(),
                                       num_channels_)) {
    std::cerr << "Failed to copy memory." << std::endl;
    return false;
  }
  return init_();
}

template<ncs::sim::DeviceType::Type MType>
bool ChannelSimulator<MType>::addChannel(void* instantiator,
                                         unsigned int neuron_plugin_id,
                                         int seed) {
  instantiators_.push_back(instantiator);
  cpu_neuron_plugin_ids_.push_back(neuron_plugin_id);
  seeds_.push_back(seed);
  return true;
}

template<ncs::sim::DeviceType::Type MType>
ChannelSimulator<MType>::~ChannelSimulator() {
  if (neuron_plugin_ids_) {
    ncs::sim::Memory<MType>::free(neuron_plugin_ids_);
  }
}

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
  result &= ncs::sim::Memory<MType>::malloc(particle_products_, 
                                            this->num_channels_);
  result &= ncs::sim::Memory<MType>::malloc(x_, num_particles_);
  result &= ncs::sim::Memory<MType>::malloc(power_, num_particles_);
  result &= ncs::sim::Memory<MType>::malloc(particle_indices_, num_particles_);
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
  float* x = new float[num_particles_];
  float* power = new float[num_particles_];
  unsigned int* particle_indices = new unsigned int[num_particles_];
  for (size_t i = 0, particle_index = 0; i < this->num_channels_; ++i) {
    auto ci = (VoltageGatedInstantiator*)(this->instantiators_[i]);
    auto seed = this->seeds_[i];
    ncs::spec::RNG rng(seed);
    conductance[i] = ci->conductance->generateDouble(&rng);
    for (auto particle_instantiator : ci->particles) {
      auto vpi = (VoltageParticleInstantiator*)particle_instantiator;
      power[particle_index] = vpi->power->generateDouble(&rng);
      x[particle_index] = vpi->x_initial->generateDouble(&rng);
      generate_particle(alpha, vpi->alpha, &rng, particle_index);
      generate_particle(beta, vpi->beta, &rng, particle_index);
      particle_indices_[particle_index] = i;
      ++particle_index;
    }
  }

  using ncs::sim::mem::copy;
  result &= copy<MType, CPU>(conductance_, conductance, this->num_channels_);
  result &= copy<MType, CPU>(x_, x, num_particles_);
  result &= copy<MType, CPU>(power_, power, num_particles_);
  result &= copy<MType, CPU>(particle_indices_,
                             particle_indices, 
                             num_particles_);
  result &= alpha_.copyFrom(&alpha, num_particles_);
  result &= beta_.copyFrom(&beta, num_particles_);
  delete [] conductance;
  delete [] x;
  delete [] power;
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
ChannelUpdater<MType>::ChannelUpdater() {
  neuron_subscription_ = nullptr;
}

template<ncs::sim::DeviceType::Type MType>
bool ChannelUpdater<MType>::
init(std::vector<ChannelSimulator<MType>*> simulators,
     ncs::sim::SpecificPublisher<NeuronBuffer<MType>>* source_publisher,
     const ncs::spec::SimulationParameters* simulation_parameters,
     size_t num_neurons,
     size_t num_buffers) {
  simulators_ = simulators;
  simulation_parameters_ = simulation_parameters;
  num_buffers_ = num_buffers;
  for (size_t i = 0; i < num_buffers_; ++i) {
    auto blank = new ChannelCurrentBuffer<MType>();
    if (!blank->init(num_neurons)) {
      std::cerr << "Failed to initialize ChannelCurrentBuffer." << std::endl;
      delete blank;
      return false;
    }
    addBlank(blank);
  }
  neuron_subscription_ = source_publisher->subscribe();
  return true;
}

template<ncs::sim::DeviceType::Type MType>
bool ChannelUpdater<MType>::start() {
  struct Synchronizer : public ncs::sim::DataBuffer {
    ChannelCurrentBuffer<MType>* channel_buffer;
    NeuronBuffer<MType>* neuron_buffer;
    float simulation_time;
    float time_step;
  };
  auto synchronizer_publisher = 
    new ncs::sim::SpecificPublisher<Synchronizer>();
  for (size_t i = 0; i < num_buffers_; ++i) {
    auto blank = new Synchronizer();
    synchronizer_publisher->addBlank(blank);
  }
  auto master_function = [this, synchronizer_publisher]() {
    float simulation_time = 0.0f;
    float time_step = simulation_parameters_->getTimeStep();
    while(true) {
      NeuronBuffer<MType>* neuron_buffer = neuron_subscription_->pull();
      if (nullptr == neuron_buffer) {
        delete synchronizer_publisher;
        return;
      }
      auto channel_buffer = this->getBlank();
      channel_buffer->clear();
      auto synchronizer = synchronizer_publisher->getBlank();
      synchronizer->channel_buffer = channel_buffer;
      synchronizer->neuron_buffer = neuron_buffer;
      synchronizer->simulation_time = simulation_time;
      synchronizer->time_step = time_step;
      auto prerelease_function = [this, channel_buffer, neuron_buffer]() {
        neuron_buffer->release();
        this->publish(channel_buffer);
      };
      synchronizer->setPrereleaseFunction(prerelease_function);
      synchronizer_publisher->publish(synchronizer);
      simulation_time += time_step;
    }
  };
  master_thread_ = std::thread(master_function);

  for (auto simulator : simulators_) {
    auto subscription = synchronizer_publisher->subscribe();
    auto worker_function = [subscription, simulator]() {
      while(true) {
        auto synchronizer = subscription->pull();
        if (nullptr == synchronizer) {
          delete subscription;
          return;
        }
        ChannelUpdateParameters parameters;
        parameters.calcium = synchronizer->neuron_buffer->getCalcium();
        parameters.voltage = synchronizer->neuron_buffer->getVoltage();
        parameters.current = synchronizer->channel_buffer->getCurrent();
        parameters.simulation_time = synchronizer->simulation_time;
        parameters.time_step = synchronizer->time_step;
        if (!simulator->update(&parameters)) {
          std::cerr << "An error occurred updating a ChannelSimulator." <<
            std::endl;
        }
        synchronizer->release();
      }
    };
    worker_threads_.push_back(std::thread(worker_function));
  }
  return true;
}

template<ncs::sim::DeviceType::Type MType>
ChannelUpdater<MType>::~ChannelUpdater() {
  if (master_thread_.joinable()) {
    master_thread_.join();
  }
  for (auto& thread : worker_threads_) {
    thread.join();
  }
  if (neuron_subscription_) {
    delete neuron_subscription_;
  }
}

template<ncs::sim::DeviceType::Type MType>
NCSSimulator<MType>::NCSSimulator() {
  // TODO(rvhoang): add calcium simulator here
  channel_simulators_.push_back(new VoltageGatedChannelSimulator<MType>());
  threshold_ = nullptr;
  resting_potential_ = nullptr;
  calcium_ = nullptr;
  calcium_spike_increment_ = nullptr;
  tau_calcium_ = nullptr;
  leak_reversal_potential_ = nullptr;
  leak_conductance_ = nullptr;
  tau_membrane_ = nullptr;
  r_membrane_ = nullptr;
  channel_current_subscription_ = nullptr;
  channel_updater_ = new ChannelUpdater<MType>();
}

template<ncs::sim::DeviceType::Type MType>
bool NCSSimulator<MType>::addNeuron(ncs::sim::Neuron* neuron) {
  neurons_.push_back(neuron);
  return true;
}

template<ncs::sim::DeviceType::Type MType>
bool NCSSimulator<MType>::
initialize(const ncs::spec::SimulationParameters* simulation_parameters) {
  using ncs::sim::Memory;
  num_neurons_ = neurons_.size();
  bool result = true;
  result &= Memory<MType>::malloc(threshold_, num_neurons_);
  result &= Memory<MType>::malloc(resting_potential_, num_neurons_);
  result &= Memory<MType>::malloc(calcium_, num_neurons_);
  result &= Memory<MType>::malloc(calcium_spike_increment_, num_neurons_);
  result &= Memory<MType>::malloc(tau_calcium_, num_neurons_);
  result &= Memory<MType>::malloc(leak_reversal_potential_, num_neurons_);
  result &= Memory<MType>::malloc(leak_conductance_, num_neurons_);
  result &= Memory<MType>::malloc(tau_membrane_, num_neurons_);
  result &= Memory<MType>::malloc(r_membrane_, num_neurons_);
  result &= Memory<MType>::malloc(voltage_persistence_, num_neurons_);
  result &= Memory<MType>::malloc(dt_capacitance_, num_neurons_);
  if (!result) {
    std::cerr << "Failed to allocate memory." << std::endl;
    return false;
  }
  
  float* threshold = new float[num_neurons_];
  float* resting_potential = new float[num_neurons_];
  float* calcium = new float[num_neurons_];
  float* calcium_spike_increment = new float[num_neurons_];
  float* tau_calcium = new float[num_neurons_];
  float* leak_reversal_potential = new float[num_neurons_];
  float* leak_conductance = new float[num_neurons_];
  float* tau_membrane = new float[num_neurons_];
  float* r_membrane = new float[num_neurons_];
  for (size_t i = 0; i < num_neurons_; ++i) {
    NeuronInstantiator* ni = (NeuronInstantiator*)(neurons_[i]->instantiator);
    ncs::spec::RNG rng(neurons_[i]->seed);
    threshold[i] = ni->threshold->generateDouble(&rng);
    resting_potential[i] = ni->resting_potential->generateDouble(&rng);
    calcium[i] = ni->calcium->generateDouble(&rng);
    calcium_spike_increment[i] = ni->calcium_spike_increment->generateDouble(&rng);
    tau_calcium[i] = ni->tau_calcium->generateDouble(&rng);
    leak_reversal_potential[i] = ni->leak_reversal_potential->generateDouble(&rng);
    leak_conductance[i] = ni->leak_conductance->generateDouble(&rng);
    tau_membrane[i] = ni->tau_membrane->generateDouble(&rng);
    r_membrane[i] = ni->r_membrane->generateDouble(&rng);
    unsigned int plugin_index = neurons_[i]->id.plugin;
    for (auto channel : ni->channels) {
      auto channel_type = channel->type;
      channel_simulators_[channel_type]->addChannel(channel, plugin_index, rng());
    }
  }
  
  float time_step = simulation_parameters->getTimeStep();
  float* voltage_persistence = new float[num_neurons_];
  float* dt_capacitance = new float[num_neurons_];
  for (size_t i = 0; i < num_neurons_; ++i) {
    float tau = tau_membrane[i];
    if (tau != 0.0f) {
      voltage_persistence[i] = 1.0f - time_step / tau;
    } else {
      voltage_persistence[i] = 1.0f;
    }
    float resistance = r_membrane[i];
    if (resistance != 0.0f) {
      float capacitance = tau / resistance;
      dt_capacitance[i] = time_step / capacitance;
    } else {
      dt_capacitance[i] = 0.0f;
    }
  }

  const auto CPU = ncs::sim::DeviceType::CPU;
  auto copy = [num_neurons_](float* src, float* dst) {
    return Memory<CPU>::To<MType>::copy(src, dst, num_neurons_);
  };
  result &= copy(threshold, threshold_);
  result &= copy(resting_potential, resting_potential_);
  result &= copy(calcium, calcium_);
  result &= copy(calcium_spike_increment, calcium_spike_increment_);
  result &= copy(tau_calcium, tau_calcium_);
  result &= copy(leak_reversal_potential, leak_reversal_potential_);
  result &= copy(leak_conductance, leak_conductance_);
  result &= copy(tau_membrane, tau_membrane_);
  result &= copy(r_membrane, r_membrane_);
  result &= copy(voltage_persistence, voltage_persistence_);
  result &= copy(dt_capacitance, dt_capacitance_);

  delete [] threshold;
  delete [] resting_potential;
  delete [] calcium;
  delete [] calcium_spike_increment;
  delete [] tau_calcium;
  delete [] leak_reversal_potential;
  delete [] leak_conductance;
  delete [] tau_membrane;
  delete [] r_membrane;
  delete [] voltage_persistence;
  delete [] dt_capacitance;

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
  ncs::sim::mem::copy<MType, MType>(blank->getCalcium(), 
                                    calcium_,
                                    num_neurons_);
  ncs::sim::mem::copy<MType, MType>(blank->getVoltage(),
                                    resting_potential_,
                                    num_neurons_);
  this->publish(blank);
  return true;
}

template<ncs::sim::DeviceType::Type MType>
bool NCSSimulator<MType>::initializeVoltages(float* plugin_voltages) {
  return ncs::sim::mem::copy<MType, MType>(plugin_voltages, 
                                           resting_potential_, 
                                           num_neurons_);
}

template<ncs::sim::DeviceType::Type MType>
bool NCSSimulator<MType>::
update(ncs::sim::NeuronUpdateParameters* parameters) {
  std::cout << "STUB: NCSSimulator<MType>::update()" << std::endl;
  return true;
}

template<ncs::sim::DeviceType::Type MType>
NCSSimulator<MType>::~NCSSimulator() {
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
  if_delete(calcium_);
  if_delete(calcium_spike_increment_);
  if_delete(tau_calcium_);
  if_delete(leak_reversal_potential_);
  if_delete(leak_conductance_);
  if_delete(tau_membrane_);
  if_delete(r_membrane_);
}
