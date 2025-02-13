/**
 * @file DirectedDevoExperiment.hpp
 * @brief Defines and manages a directed evolution experiment.
 *
 * DIRDEVO_THREADING
 */

#pragma once
#ifndef DIRECTED_DEVO_DIRECTED_DEVO_EXPERIMENT_HPP_INCLUDE
#define DIRECTED_DEVO_DIRECTED_DEVO_EXPERIMENT_HPP_INCLUDE

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <functional>
#include <filesystem>
#include <sys/stat.h>

#include "emp/base/vector.hpp"
#include "emp/datastructs/vector_utils.hpp"
#include "emp/Evolve/World.hpp"
#include "emp/Evolve/Systematics.hpp"
#include "emp/tools/string_utils.hpp"
#include "emp/data/DataFile.hpp"

#include "DirectedDevoConfig.hpp"
#include "DirectedDevoWorld.hpp"
#include "BasePeripheral.hpp"             /// TODO - fully integrate the peripheral component!
#include "selection/SelectionSchemes.hpp"
#include "selection/BaseSelect.hpp"
#include "utility/ConfigSnapshotEntry.hpp"
#include "utility/WorldAwareDataFile.hpp"

#ifdef DIRDEVO_THREADING
#include <thread>
#include <mutex>
#endif // DIRDEVO_THREADED

namespace dirdevo {

// TODO - we're using one uniform configuration type, so just hand off configs and let things configure themselves.
// TODO - make communication between experiment and components more consistent (e.g., Configuration; let components configure themselves?)

const size_t MAX_WORLD_SEED = 100000;

// PERIPHERAL defines any extra equipment needed to run the experiment (typically something required by the subtasks)
template<typename WORLD, typename ORG, typename MUTATOR, typename TASK, typename PERIPHERAL=BasePeripheral>
class DirectedDevoExperiment {
public:
  struct TransferOrg;

  // --- Publically available types ---
  using this_t = DirectedDevoExperiment<WORLD,ORG,MUTATOR,TASK,PERIPHERAL>;
  using org_t = ORG;
  using task_t = TASK;
  using world_t = WORLD; //DirectedDevoWorld<org_t,task_t>;
  using config_t = DirectedDevoConfig;
  using pop_struct_t = typename world_t::POP_STRUCTURE;
  using peripheral_t = PERIPHERAL;
  using world_aware_data_file_t = WorldAwareDataFile<world_t>;

  using mutator_t = MUTATOR;
  using genome_t = typename org_t::genome_t;
  using propagule_t = emp::vector<TransferOrg>;

  // TODO - add mutation tracking to systematics?
  using systematics_t = emp::Systematics<org_t, genome_t>;

  const std::unordered_set<std::string> valid_selection_methods={
    "elite",
    "tournament",
    "lexicase",
    "non-dominated-elite",
    "non-dominated-tournament",
    "random",
    "none"
  };

  const std::unordered_set<std::string> valid_population_sampling_methods={
    "random",
    "full"
  };

  /// Propagules are vectors of TransferGenomes. A TransferGenome wraps information about the genomes sampled to form propagules.
  /// Necessary for stitching together phylogeny tracking across transfers.
  struct TransferOrg {
    emp::Ptr<org_t> org;
    size_t original_pos=0;
    size_t transfer_pos=0;
  };

protected:

  const config_t& config;                  ///< Experiment configuration (REMINDER: the config object must exist beyond lifetime of this experiment object!)
  emp::Random random;                      ///< Experiment-level random number generator.
  emp::vector<emp::Random> world_rngs;    ///< To minimize shared memory resources between worlds (for threading), each world gets its own (uniquely seeded) random number generator.

  emp::vector<emp::Ptr<world_t>> worlds;   ///< How many "populations" are we applying directed evolution to?

  pop_struct_t local_pop_struct=pop_struct_t::MIXED;
  // mutator_t mutator;
  emp::vector<mutator_t> mutators;                 ///< One mutator per world. (to avoid shared memory resources for threading)
  peripheral_t peripheral;                      ///< Peripheral components that should exist at the experiment level.
  emp::Ptr<systematics_t> systematics=nullptr;  ///< Phylogeny tracking

  emp::Ptr<BaseSelect> selector=nullptr;

  std::function<emp::vector<size_t>&(void)> do_selection_fun;
  emp::vector<std::function<double(void)>> aggregate_score_funs;          ///< One function for each world.
  emp::vector< emp::vector<std::function<double(void)>> > score_fun_sets; ///< One set of functions for each world. Where each function corresponds to a single objective.

  std::function<void(world_t&,propagule_t&)> propagule_sample_fun;
  emp::vector<propagule_t> propagules;
  std::unordered_set<size_t> extinct_worlds;        ///< Set of worlds that are extinct.
  std::unordered_set<size_t> live_worlds;           ///< Set of worlds that are not extinct.
  emp::vector<size_t> population_sample_order;

  emp::map<typename emp::World<ORG>::genome_t, size_t> genomes_seen; // Sydney: store genomes for mapping ids
  emp::map<size_t, emp::map<size_t, float>> interaction_matrix; // Sydney: store interaction matrix for a given world_id and epoch
  size_t interaction_matrix_world_id; // Sydney: for data file

  size_t max_world_size=0;
  bool setup=false;
  size_t cur_epoch=0;
  bool record_epoch=false;

  emp::Ptr<world_aware_data_file_t> world_summary_file=nullptr;     ///< Manages world update summary output. (is updated during world updates; for each world)
  emp::Ptr<emp::DataFile> world_evaluation_file=nullptr;  ///< Manages world evaluation output. (is updated after each world's evaluation)
  emp::Ptr<emp::DataFile> world_systematics_file=nullptr; ///<
  emp::Ptr<emp::DataFile> interaction_matrices_file=nullptr; ///< Sydney: stores interaction matrices

  std::string output_dir;                     ///< Formatted output directory

  /// Setup the experiment based on the given configuration (called internally).
  void Setup();

  /// Configure local population structure (called internally).
  // void SetLocalPopStructure();

  /// Configure population selection (called internally).
  void SetupSelection();
  void SetupSystematics();
  void SetupEliteSelection();
  void SetupTournamentSelection();
  void SetupLexicaseSelection();
  void SetupNonDominatedEliteSelection();
  void SetupNonDominatedTournamentSelection();
  void SetupRandomSelection();
  void SetupNoSelection();

  void SetupPropaguleSampleMethod();

  /// Configure data collection
  void SetupDataCollection();

  // TODO - allow for different sampling techniques / ways of forming propagules
  // - e.g., each propagules comes from a single world? each propagule is a mixture of all worlds?
  //        'propagule' crossover?
  void Sample(world_t& world, propagule_t& sample_into); // NOTE - should this live in the experiment or the world class?

  void SeedWithPropagule(world_t& world, propagule_t& propagule);

  /// Output the experiment's configuration as a .csv file.
  void SnapshotConfig(const std::string& filename = "experiment-config.csv");

  /// Return whether configuration is valid.
  bool ValidateConfig();

public:

  DirectedDevoExperiment(
    const config_t& in_config
  ) :
    config(in_config),
    random(config.SEED())
  {
     Setup(); // TODO - should setup be in constructor or controlled externally?
  }

  ~DirectedDevoExperiment() {
    // Clean up worlds
    aggregate_score_funs.clear();
    for (auto world : worlds) {
      if (world != nullptr) world.Delete();
    }

    // Clean up data files
    if (world_summary_file) world_summary_file.Delete();
    if (world_evaluation_file) world_evaluation_file.Delete();
    if (world_systematics_file) world_systematics_file.Delete();
    if (interaction_matrices_file) interaction_matrices_file.Delete(); // Sydney

    // Clean up any undeleted propagule organism pointers
    for (propagule_t& propagule : propagules) {
      for (size_t i = 0; i < propagule.size(); ++i) {
        if (propagule[i].org) propagule[i].org.Delete();
      }
    }

    // Clean up the shared (between worlds) systematics manager
    if (systematics) systematics.Delete();

    // Clean up the selector
    if (selector) selector.Delete();
  }

  /// Run experiment for configured number of EPOCHS
  void Run();

  /// Advance each world by one step (advance each world by a single step)
  /// Note that RunStep does not take into account epochs or updates per epoch.
  /// - Used primarily for testing and the web interface. Use Run to run the experiment.
  void RunStep();

  /// Sydney
  emp::map<typename emp::World<ORG>::genome_t, float> GetWorldFitnesses(emp::map<typename emp::World<ORG>::genome_t, size_t> unique_genomes, size_t world_id, size_t remove_cell);

  peripheral_t& GetPeripheral() { return peripheral; }
  const peripheral_t& GetPeripheral() const { return peripheral; }

};

template <typename WORLD, typename ORG, typename MUTATOR, typename TASK, typename PERIPHERAL>
void DirectedDevoExperiment<WORLD, ORG, MUTATOR, TASK, PERIPHERAL>::Setup() {
  if (setup) return; // Don't let myself run Setup more than once.

  #ifdef DIRDEVO_THREADING
  std::cout << "Compiled with threading enabled." << std::endl;
  #endif // DIRDEVO_THREADING

  // Validate configuration (even in when compiled outside of debug mode!)
  if(!ValidateConfig()) {
    // todo - report which configs are invalid?
    std::cout << "Invalid configuration, exiting." << std::endl;
    std::exit(EXIT_FAILURE);
  }

  // What population structure are we using?
  // TODO - clean this up a bit!
  // local_pop_struct = world_t::PopStructureStrToMode(config.LOCAL_POP_STRUCTURE());
  // typename world_t::PopStructureDesc pop_struct(local_pop_struct, config.LOCAL_GRID_WIDTH(), config.LOCAL_GRID_HEIGHT(), config.LOCAL_GRID_DEPTH());

  // Configure the mutator
  mutators.resize(config.NUM_POPS());
  for (auto& mutator : mutators) {
    mutator_t::Configure(mutator, config);
  }

  // Configure the peripheral components
  peripheral.Setup(config);

  #ifdef DIRDEVO_THREADING
  // NOTE - if number of populations is close to max world seed, this loop might take a really long time...
  emp_assert(MAX_WORLD_SEED > config.NUM_POPS());
  std::unordered_set<size_t> world_seeds;
  while (world_seeds.size() < config.NUM_POPS()) {
    world_seeds.emplace(random.GetUInt());
  }
  for (auto seed : world_seeds) {
    world_rngs.emplace_back(seed);
  }
  #endif // DIRDEVO_THREADING

  // Initialize each world.
  worlds.resize(config.NUM_POPS());
  max_world_size=0;
  for (size_t i = 0; i < config.NUM_POPS(); ++i) {
    worlds[i] = emp::NewPtr<world_t>(
      config,
      #ifdef DIRDEVO_THREADING
      world_rngs[i],
      #else
      random,
      #endif // DIRDEVO_THREADING
      "world_"+emp::to_string(i),
      i
    );
    worlds[i]->SetAvgOrgStepsPerUpdate(config.AVG_STEPS_PER_ORG());
    // configure world's mutation function
    worlds[i]->SetMutFun([this, i](org_t & org, emp::Random& rnd) {
      // TODO - add support for mutation tracking!
      return mutators[i].Mutate(org.GetGenome(), rnd);
    });
    max_world_size = emp::Max(worlds[i]->GetSize(), max_world_size);
  }

  if (config.TRACK_SYSTEMATICS()) {
    SetupSystematics();
  }

  // Seed each world with an initial common ancestor
  // PROBLEM - can't do out-of-world injection for genomes
  for (auto world_ptr : worlds) {
    std::function<genome_t(this_t&, world_t&)> get_ancestor_genome;
    // TODO - Make ancestor loading a little smarter? E.g., allow different ancestors for different worlds? Can hack into the load function (using the world name to differentiate).
    if (config.LOAD_ANCESTOR_FROM_FILE()) {
      // check that file exists
      if (!std::filesystem::exists(config.ANCESTOR_FILE())) {
        std::cout << "Ancestor file does not exist. " << config.ANCESTOR_FILE() << std::endl;
        std::exit(EXIT_FAILURE);
      }
      get_ancestor_genome = [](this_t& experiment, world_t& world) {
        return org_t::LoadAncestralGenome(experiment, world);
      };
    } else {
      get_ancestor_genome = [](this_t& experiment, world_t& world) {
        return org_t::GenerateAncestralGenome(experiment, world);
      };
    }
    // auto ancestral_genome = ;
    world_ptr->InjectAt(get_ancestor_genome(*this, *world_ptr), 0); // TODO - Random location to start?
  }

  // Adjust initial scheduler weights according to initial population!
  for (auto world_ptr : worlds) {
    world_ptr->SyncSchedulerWeights();
  }

  // Setup selection
  SetupSelection();

  // Setup propagule sampling method
  SetupPropaguleSampleMethod();

  // Setup data collection
  SetupDataCollection();

  // TODO - should config snapshot be here or elsewhere?
  SnapshotConfig();
  setup = true;
}

template <typename WORLD, typename ORG, typename MUTATOR, typename TASK, typename PERIPHERAL>
void DirectedDevoExperiment<WORLD, ORG, MUTATOR, TASK, PERIPHERAL>::SetupSelection() {

  // Wire up aggregate score functions
  aggregate_score_funs.clear();
  for (size_t pop_id = 0; pop_id < config.NUM_POPS(); ++pop_id) {
    aggregate_score_funs.emplace_back(
      [this, pop_id]() {
        return worlds[pop_id]->GetAggregateTaskPerformance();
      }
    );
  }

  // Wire up function sets
  score_fun_sets.clear();
  std::unordered_set<size_t> fun_set_sizes;
  for (size_t pop_id = 0; pop_id < config.NUM_POPS(); ++pop_id) {
    score_fun_sets.emplace_back();
    const size_t fun_set_size = worlds[pop_id]->GetNumSubTasks();
    fun_set_sizes.emplace(fun_set_size);
    for (size_t fun_i = 0; fun_i < worlds[pop_id]->GetNumSubTasks(); ++fun_i) {
      score_fun_sets[pop_id].emplace_back(
        [this, pop_id, fun_i] () {
          return worlds[pop_id]->GetSubTaskPerformance(fun_i);
        }
      );
    }
  }
  emp_assert(fun_set_sizes.size() == 1, "Not all worlds have same number of sub task performance functions");

  if (config.SELECTION_METHOD() == "elite") {
    SetupEliteSelection();
  } else if (config.SELECTION_METHOD() == "tournament") {
    SetupTournamentSelection();
  } else if (config.SELECTION_METHOD() == "lexicase") {
    SetupLexicaseSelection();
  } else if (config.SELECTION_METHOD() == "non-dominated-elite") {
    SetupNonDominatedEliteSelection();
  } else if (config.SELECTION_METHOD() == "non-dominated-tournament") {
    SetupNonDominatedTournamentSelection();
  } else if (config.SELECTION_METHOD() == "random") {
    SetupRandomSelection();
  } else if (config.SELECTION_METHOD() == "none") {
    SetupNoSelection();
  } else {
    // code should never reach this else (unless I forget to add a selection scheme here that is in the valid selection method set)
    emp_assert(false, "Unimplemented selection scheme.", config.SELECTION_METHOD());
  }
}

template <typename WORLD, typename ORG, typename MUTATOR, typename TASK, typename PERIPHERAL>
void DirectedDevoExperiment<WORLD, ORG, MUTATOR, TASK, PERIPHERAL>::SetupPropaguleSampleMethod() {

  population_sample_order.resize(max_world_size);
  std::iota(
    population_sample_order.begin(),
    population_sample_order.end(),
    0
  );

  if (config.POPULATION_SAMPLING_METHOD() == "random") {
    // Sample randomly
    propagule_sample_fun = [this](world_t& world, propagule_t& sample_into) {
      sample_into.clear();
      emp::Shuffle(world.GetRandom(), population_sample_order);
      // extinct worlds shouldn't get selected (unless everything went extinct or we're doing random selection...)
      for (size_t i = 0; (i < population_sample_order.size()) && (sample_into.size() < config.POPULATION_SAMPLING_SIZE()); ++i) {
        const size_t sampled_pos = population_sample_order[i];
        if (!world.IsOccupied({sampled_pos})) continue;
        emp_assert(world.IsOccupied({sampled_pos}));
        // const size_t sampled_pos = world.GetRandomOrgID();
        const size_t world_pos_offset = world.GetSharedSystematics().offset; // 0 if not tracking systematics
        sample_into.emplace_back();
        sample_into.back().org = emp::NewPtr<org_t>(world.GetOrg(sampled_pos).GetGenome());
        sample_into.back().original_pos = world_pos_offset + sampled_pos;
      }
    };

  } else if (config.POPULATION_SAMPLING_METHOD() == "full") {
    // Sample everything
    propagule_sample_fun = [this](world_t& world, propagule_t& sample_into) {
      sample_into.clear();
      for (size_t org_id = 0; org_id < world.GetSize(); ++org_id) {
        if (!world.IsOccupied({org_id})) continue;
        const size_t sampled_pos = org_id;
        const size_t world_pos_offset = world.GetSharedSystematics().offset; // 0 if not tracking systematics
        sample_into.emplace_back();
        sample_into.back().org = emp::NewPtr<org_t>(world.GetOrg(sampled_pos).GetGenome());
        sample_into.back().original_pos = world_pos_offset + sampled_pos;
      }
    };
  } else {
    // code should never reach this else (unless I forget to add a selection scheme here that is in the valid selection method set)
    emp_assert(false, "Unimplemented propagule sampling method.", config.POPULATION_SAMPLING_METHOD());
  }
}

template <typename WORLD, typename ORG, typename MUTATOR, typename TASK, typename PERIPHERAL>
void DirectedDevoExperiment<WORLD, ORG, MUTATOR, TASK, PERIPHERAL>::SetupSystematics() {
  // Configure systematics tracking (TODO - allow systematics tracking to be stripped out for performance)
  systematics = emp::NewPtr<systematics_t>([](const org_t& org) { return org.GetGenome(); });
  systematics->SetTrackSynchronous(false); // Tell systematics that we have asynchronous generations
  systematics->AddPairwiseDistanceDataNode();
  systematics->AddPhylogeneticDiversityDataNode();
  for (auto world_ptr : worlds) {
    world_ptr->SetSharedSystematics(systematics, max_world_size);
  }
}

template <typename WORLD, typename ORG, typename MUTATOR, typename TASK, typename PERIPHERAL>
void DirectedDevoExperiment<WORLD, ORG, MUTATOR, TASK, PERIPHERAL>::SetupDataCollection() {
  output_dir = config.OUTPUT_DIR();
  if (setup) {
    // anything we need to do if this function is called post-setup
    if (world_summary_file) world_summary_file.Delete();
    if (world_evaluation_file) world_evaluation_file.Delete();
    if (world_systematics_file) world_systematics_file.Delete();
    if (interaction_matrices_file) interaction_matrices_file.Delete(); // Sydney
  } else {
    mkdir(output_dir.c_str(), ACCESSPERMS);
    if(output_dir.back() != '/') {
      output_dir += '/';
    }
  }

  // Generally useful functions
  std::function<size_t(void)> get_epoch = [this]() { return cur_epoch; };

  // Sydney: save interaction matrices
  interaction_matrices_file = emp::NewPtr<emp::DataFile>(output_dir + "interaction_matrices.csv");
  interaction_matrices_file->AddVar(cur_epoch, "epoch");
  interaction_matrices_file->AddVar(interaction_matrix_world_id, "world");
  interaction_matrices_file->AddFun<std::string>(
    [this]() {
      size_t i;
      size_t j = 1;
      std::ostringstream stream;
      stream << "\"{";
      for (auto& [genome_i, genome_i_row] : interaction_matrix) {
        stream << genome_i << ":{";
        i = 1;
        for (auto& [genome_j, interaction] : genome_i_row) {
          stream << genome_j << ":" << interaction;
          if (i != genome_i_row.size()) stream << ", ";
          i++;
        }
        stream << "}";
        if (j != interaction_matrix.size()) stream << ", ";
        j++;
      }
      stream << "}\"";
      return stream.str();
    },
    "matrix"
  );
  interaction_matrices_file->PrintHeaderKeys();

  //////////////////////////////////
  // WORLD UPDATE SUMMARY
  if (config.OUTPUT_COLLECT_WORLD_UPDATE_SUMMARY()) {
    // TODO - rename world_summary file and associated functions?
    world_summary_file = emp::NewPtr<world_aware_data_file_t>(output_dir + "world_summary.csv");
    // Experiment level functions
    world_summary_file->template AddFun<size_t>(get_epoch,"epoch");
    // World-level functions
    world_t::AttachWorldUpdateDataFileFunctions(*world_summary_file);
    world_summary_file->PrintHeaderKeys();
  }

  //////////////////////////////////
  // WORLD EVALUATION
  world_evaluation_file = emp::NewPtr<emp::DataFile>(output_dir + "world_evaluation.csv");
  // Experiment level functions
  // epoch
  world_evaluation_file->AddFun<size_t>(get_epoch, "epoch");

  // aggregate scores
  world_evaluation_file->AddFun<std::string>(
    [this]() {
      std::ostringstream stream;
      stream << "\"[";
      for (size_t i = 0; i < worlds.size(); ++i) {
        if (i) stream << ",";
        stream << aggregate_score_funs[i]();
      }
      stream << "]\"";
      return stream.str();
    },
    "aggregate_scores"
  );

  // scores (by world, by function)
  world_evaluation_file->AddFun<std::string>(
    [this]() {
      std::ostringstream stream;
      stream << "\"[[";
      for (size_t i = 0; i < worlds.size(); ++i) {
        emp_assert(i < score_fun_sets.size());
        if (i) stream << ",[";
        for (size_t fun_i = 0; fun_i < score_fun_sets[i].size(); ++fun_i) {
          if (fun_i) stream << ",";
          stream << score_fun_sets[i][fun_i]();
        }
        stream << "]";
      }
      stream << "]\"";
      return stream.str();
    },
    "scores"
  );

  // selected
  world_evaluation_file->AddFun<std::string>(
    [this]() {
      std::ostringstream stream;
      stream << "\"[";
      const auto& selected = selector->GetSelected();
      for (size_t i = 0; i < selected.size(); ++i) {
        if (i) stream << ",";
        stream << selected[i];
      }
      stream << "]\"";
      return stream.str();
    },
    "selected"
  );

  // unique selected
  world_evaluation_file->AddFun<size_t>(
    [this]() {
      const auto& selected = selector->GetSelected();
      return std::unordered_set<size_t>(selected.begin(), selected.end()).size();
    },
    "num_unique_selected"
  );

  world_evaluation_file->PrintHeaderKeys();

  //////////////////////////////////
  // Systematics
  if (config.TRACK_SYSTEMATICS()) {
    // basic stuff
    world_systematics_file = emp::NewPtr<emp::DataFile>(output_dir + "systematics.csv");
    world_systematics_file->AddVar(cur_epoch, "epoch");
    world_systematics_file->AddFun<size_t>( [this](){ return systematics->GetNumActive(); }, "num_taxa", "Number of unique taxonomic groups currently active." );
    world_systematics_file->AddFun<size_t>( [this](){ return systematics->GetTotalOrgs(); }, "total_orgs", "Number of organisms tracked." );
    world_systematics_file->AddFun<double>( [this](){ return systematics->GetAveDepth(); }, "ave_depth", "Average Phylogenetic Depth of Organisms." );
    world_systematics_file->AddFun<size_t>( [this](){ return systematics->GetNumRoots(); }, "num_roots", "Number of independent roots for phylogenies." );
    world_systematics_file->AddFun<int>(    [this](){ return systematics->GetMRCADepth(); }, "mrca_depth", "Phylogenetic Depth of the Most Recent Common Ancestor (-1=none)." );
    world_systematics_file->AddFun<double>( [this](){ return systematics->CalcDiversity(); }, "diversity", "Genotypic Diversity (entropy of taxa in population)." );
    // phylodiversity
    world_systematics_file->AddStats(*systematics->GetDataNode("pairwise_distance"), "genotype_pairwise_distance", "pairwise distance for a single update", true, true);
    world_systematics_file->AddCurrent(*systematics->GetDataNode("phylogenetic_diversity"), "genotype_current_phylogenetic_diversity", "current phylogenetic_diversity", true, true);
    // write file header
    world_systematics_file->PrintHeaderKeys();
  }

}

template <typename WORLD, typename ORG, typename MUTATOR, typename TASK, typename PERIPHERAL>
void DirectedDevoExperiment<WORLD, ORG, MUTATOR, TASK, PERIPHERAL>::SetupEliteSelection() {
  selector = emp::NewPtr<EliteSelect>(
    aggregate_score_funs,
    config.ELITE_SEL_NUM_ELITES()
  );

  do_selection_fun = [this]() -> emp::vector<size_t>& {
    emp::Ptr<EliteSelect> sel = selector.Cast<EliteSelect>();
    return (*sel)(config.NUM_POPS());
  };
}

template <typename WORLD, typename ORG, typename MUTATOR, typename TASK, typename PERIPHERAL>
void DirectedDevoExperiment<WORLD, ORG, MUTATOR, TASK, PERIPHERAL>::SetupTournamentSelection() {
  selector = emp::NewPtr<TournamentSelect>(
    aggregate_score_funs,
    random,
    config.TOURNAMENT_SEL_TOURN_SIZE()
  );

  do_selection_fun = [this]() -> emp::vector<size_t>& {
    emp::Ptr<TournamentSelect> sel = selector.Cast<TournamentSelect>();
    return (*sel)(config.NUM_POPS());
  };
}

template <typename WORLD, typename ORG, typename MUTATOR, typename TASK, typename PERIPHERAL>
void DirectedDevoExperiment<WORLD, ORG, MUTATOR, TASK, PERIPHERAL>::SetupLexicaseSelection() {
  selector = emp::NewPtr<LexicaseSelect>(
    score_fun_sets,
    random
  );

  do_selection_fun = [this]() -> emp::vector<size_t>& {
    emp::Ptr<LexicaseSelect> sel = selector.Cast<LexicaseSelect>();
    return (*sel)(config.NUM_POPS());
  };
}

template <typename WORLD, typename ORG, typename MUTATOR, typename TASK, typename PERIPHERAL>
void DirectedDevoExperiment<WORLD, ORG, MUTATOR, TASK, PERIPHERAL>::SetupNonDominatedEliteSelection() {
  selector = emp::NewPtr<NonDominatedEliteSelect>(
    score_fun_sets,
    random
  );

  do_selection_fun = [this]() -> emp::vector<size_t>& {
    emp::Ptr<NonDominatedEliteSelect> sel = selector.Cast<NonDominatedEliteSelect>();
    return (*sel)(config.NUM_POPS());
  };
}

template <typename WORLD, typename ORG, typename MUTATOR, typename TASK, typename PERIPHERAL>
void DirectedDevoExperiment<WORLD, ORG, MUTATOR, TASK, PERIPHERAL>::SetupNonDominatedTournamentSelection() {
  selector = emp::NewPtr<NonDominatedTournamentSelect>(
    score_fun_sets,
    random,
    config.TOURNAMENT_SEL_TOURN_SIZE()
  );

  do_selection_fun = [this]() -> emp::vector<size_t>& {
    emp::Ptr<NonDominatedTournamentSelect> sel = selector.Cast<NonDominatedTournamentSelect>();
    return (*sel)(config.NUM_POPS());
  };
}

template <typename WORLD, typename ORG, typename MUTATOR, typename TASK, typename PERIPHERAL>
void DirectedDevoExperiment<WORLD, ORG, MUTATOR, TASK, PERIPHERAL>::SetupRandomSelection() {
  selector = emp::NewPtr<RandomSelect>(
    random,
    config.NUM_POPS()
  );

  do_selection_fun = [this]() -> emp::vector<size_t>& {
    emp::Ptr<RandomSelect> sel = selector.Cast<RandomSelect>();
    return (*sel)(config.NUM_POPS());
  };
}

template <typename WORLD, typename ORG, typename MUTATOR, typename TASK, typename PERIPHERAL>
void DirectedDevoExperiment<WORLD, ORG, MUTATOR, TASK, PERIPHERAL>::SetupNoSelection() {
  selector = emp::NewPtr<NoSelect>();

  do_selection_fun = [this]() -> emp::vector<size_t>& {
    emp::Ptr<NoSelect> sel = selector.Cast<NoSelect>();
    return (*sel)(config.NUM_POPS());
  };
}

template <typename WORLD, typename ORG, typename MUTATOR, typename TASK, typename PERIPHERAL>
// emp::vector<typename DirectedDevoExperiment<WORLD, ORG, MUTATOR, TASK, PERIPHERAL>::TransferOrg>
void DirectedDevoExperiment<WORLD, ORG, MUTATOR, TASK, PERIPHERAL>::Sample(world_t& world, propagule_t& sample_into) {
  emp_assert(!world.IsExtinct(), "Attempting to sample from an extinct population.");
  propagule_sample_fun(world, sample_into);
}

template <typename WORLD, typename ORG, typename MUTATOR, typename TASK, typename PERIPHERAL>
void DirectedDevoExperiment<WORLD, ORG, MUTATOR, TASK, PERIPHERAL>::SeedWithPropagule(world_t& world, propagule_t& propagule) {
  // TODO - Tweak if we ever complicate how propagules are seeded into the world
  emp_assert(propagule.size() <= world.GetSize(), "Propagule size cannot exceed world size.", propagule.size(), world.GetSize());
  for (size_t i = 0; i < propagule.size(); ++i) {
   const size_t pos = i; // TODO - use a slightly better method of distributing the propagule!
   // need to set next parent
   if (config.TRACK_SYSTEMATICS()) systematics->SetNextParent(propagule[i].transfer_pos);
   world.InjectAt(propagule[i].org->GetGenome(), {pos});
  }
  world.SyncSchedulerWeights();
}

template <typename WORLD, typename ORG, typename MUTATOR, typename TASK, typename PERIPHERAL>
void DirectedDevoExperiment<WORLD, ORG, MUTATOR, TASK, PERIPHERAL>::SnapshotConfig(
  const std::string& filename /*= "experiment-config.csv"*/
) {

  emp::DataFile snapshot_file(output_dir + "/run_config.csv");
  std::function<std::string(void)> get_param;
  std::function<std::string(void)> get_value;
  std::function<std::string(void)> get_source;
  snapshot_file.AddFun<std::string>(
    [&get_param]() { return get_param(); },
    "parameter"
  );
  snapshot_file.AddFun<std::string>(
    [&get_value]() { return get_value(); },
    "value"
  );
  snapshot_file.AddFun<std::string>(
    [&get_source]() { return get_source(); },
    "source"
  );

  snapshot_file.PrintHeaderKeys();

  // CUSTOM STUFF would go here!

  // Snapshot config
  get_source = []() { return "experiment"; };
  for (const auto & entry : config) {
    get_param = [&entry]() { return entry.first; };
    get_value = [&entry]() { return emp::to_string(entry.second->GetValue()); };
    snapshot_file.Update();
  }

  // Collect and snapshot downstream configuration
  emp::vector<ConfigSnapshotEntry> entries;
  for (auto world_ptr : worlds) {
    auto world_entries = world_ptr->GetConfigSnapshotEntries();
    std::copy(world_entries.begin(), world_entries.end(), std::back_inserter(entries));
  }
  for (const auto& entry : entries) {
    get_param = [&entry]() { return entry.param; };
    get_value = [&entry]() { return entry.value; };
    get_source = [&entry]() { return entry.source; };
    snapshot_file.Update();
  }

}

template <typename WORLD, typename ORG, typename MUTATOR, typename TASK, typename PERIPHERAL>
bool DirectedDevoExperiment<WORLD, ORG, MUTATOR, TASK, PERIPHERAL>::ValidateConfig() {
  // GLOBAL SETTINGS
  if (config.NUM_POPS() < 1) return false;
  // LOCAL WORLD SETTINGS
  if (!world_t::IsValidPopStructure(config.LOCAL_POP_STRUCTURE())) return false;
  if (config.LOCAL_GRID_WIDTH() < 1) return false;
  if (config.LOCAL_GRID_HEIGHT() < 1) return false;
  if (config.LOCAL_GRID_DEPTH() < 1) return false;
  if (config.AVG_STEPS_PER_ORG() < 1) return false;
  if (!emp::Has(valid_selection_methods,config.SELECTION_METHOD())) return false;
  if (config.POPULATION_SAMPLING_SIZE() < 1) return false;
  // TODO - flesh this out!

  #ifdef DIRDEVO_THREADING
  if (config.TRACK_SYSTEMATICS()) {
    std::cout << "Cannot track systematics if compiled with threading enabled." << std::endl;
    return false;
  }
  #endif

  return true;
}


template <typename WORLD, typename ORG, typename MUTATOR, typename TASK, typename PERIPHERAL>
void DirectedDevoExperiment<WORLD, ORG, MUTATOR, TASK, PERIPHERAL>::Run() {
  // Create vector to hold the distribution of population ids selected each epoch

  #ifdef DIRDEVO_THREADING
  std::function<void(size_t)> run_world = [this](size_t world_id) {
    worlds[world_id]->SetEpoch(cur_epoch);
    for (size_t u = 0; u <= config.UPDATES_PER_EPOCH(); u++) {
      worlds[world_id]->RunStep();
      worlds[world_id]->Update();
    }
  };
  #endif // DIRDEVO_THREADING

  for (cur_epoch = 0; cur_epoch <= config.EPOCHS(); ++cur_epoch) {
    std::cout << "==== EPOCH " << cur_epoch << " ====" << std::endl;

    // Refresh epoch-level bookkeeping
    extinct_worlds.clear();
    live_worlds.clear();

    // Is this an epoch that we want to record data for?
    // - Either correct interval or final epoch.
    record_epoch = !(cur_epoch % config.OUTPUT_SUMMARY_EPOCH_RESOLUTION()) || (cur_epoch == config.EPOCHS());
    const bool snapshot_phylogeny = config.TRACK_SYSTEMATICS() && (!(cur_epoch % config.OUTPUT_PHYLOGENY_SNAPSHOT_EPOCH_RESOLUTION()) || (cur_epoch == config.EPOCHS()));
    const bool record_systematics = config.TRACK_SYSTEMATICS() && (!(cur_epoch % config.OUTPUT_SYSTEMATICS_EPOCH_RESOLUTION()) || (cur_epoch == config.EPOCHS()));

    #ifdef DIRDEVO_THREADING
    ///////////////////////////////////////////////
    // THREADING ENABLED
    emp::vector<std::thread> threads;
    for (size_t world_id = 1; world_id < worlds.size(); ++world_id) {
      threads.emplace_back(
        run_world,
        world_id
      );
    }
    run_world(0); // use the main thread to run world 0
    // Join threads
    for (auto& thread : threads) {
      thread.join();
    }
    // Update world summary file
    for (auto world_ptr : worlds) {
      world_summary_file->Update(world_ptr);
    }
    ///////////////////////////////////////////////
    #else
    ///////////////////////////////////////////////
    // THREADING DISABLED
    // Run worlds forward X updates.
    for (auto world_ptr : worlds) {
      std::cout << "Running world " << world_ptr->GetName() << std::endl;
      world_ptr->SetEpoch(cur_epoch);
      for (size_t u = 0; u <= config.UPDATES_PER_EPOCH(); u++) {
        const bool record_update = config.OUTPUT_COLLECT_WORLD_UPDATE_SUMMARY() && (!(u % config.OUTPUT_SUMMARY_UPDATE_RESOLUTION()) || (u == config.UPDATES_PER_EPOCH()));
        if (record_update) {
          world_summary_file->Update(world_ptr);
        }
        world_ptr->Update();
      }
    }
    ///////////////////////////////////////////////
    #endif //DIRDEVO_THREADING

    // Sydney: calculate interaction matrix
    if (cur_epoch == 499) { // TEMP
      for (size_t world_id = 0; world_id < worlds.size(); ++world_id) {
        interaction_matrix_world_id = world_id;

        //get genomes in world
        emp::map<typename emp::World<ORG>::genome_t, size_t> unique_genomes;
        for (size_t i=0; i < worlds[world_id]->GetSize(); i++) {
          if (worlds[world_id]->IsOccupied(i)) {
            typename emp::World<ORG>::genome_t org_genome = worlds[world_id]->GetOrg(i).GetGenome();
            if (unique_genomes.find(org_genome) == unique_genomes.end()) {
              unique_genomes.insert({org_genome, i});
            }
            //global mapping of genomes to ids
            if (genomes_seen.find(org_genome) == genomes_seen.end()) {
              genomes_seen.insert({org_genome, genomes_seen.size()});
            }
          }
        }

        //fill out interaction matrix
        interaction_matrix.clear();
        emp::map<typename emp::World<ORG>::genome_t, float> default_fitnesses = GetWorldFitnesses(unique_genomes, world_id, -1);
        for (auto& [remove_genome, remove_cell] : unique_genomes) {
          emp::map<typename emp::World<ORG>::genome_t, float> genome_fitnesses = GetWorldFitnesses(unique_genomes, world_id, remove_cell);
          for (auto& [genome, cell] : unique_genomes) {
            if (remove_genome == genome) {
              interaction_matrix[genomes_seen[genome]][genomes_seen[remove_genome]] = 0;
            }
            else {
              interaction_matrix[genomes_seen[genome]][genomes_seen[remove_genome]] = genome_fitnesses[genome] - default_fitnesses[genome];
            }
          }
        }
        interaction_matrices_file->Update();
      }
    }

    // Do evaluation (could move this into previous loop if I don't add anything else here that requires all worlds to have been run)
    for (size_t world_id = 0; world_id < worlds.size(); ++world_id) {
      worlds[world_id]->Evaluate();
      (worlds[world_id]->IsExtinct()) ? extinct_worlds.insert(world_id) : live_worlds.insert(world_id);
    }

    const bool all_worlds_extinct = extinct_worlds.size() == worlds.size();

    // Snapshot the phylogeny?
    if (snapshot_phylogeny) {
      systematics->Snapshot(output_dir + "phylogeny_" + emp::to_string(cur_epoch) + ".csv");
    }

    // Record systematics?
    if (record_systematics) {
      world_systematics_file->Update();
    }

    if (all_worlds_extinct) {
      std::cout << "All of the worlds are extinct." << std::endl;
      // TODO - when exiting because all worlds are extinct, cleanly do snapshot, etc
      break;
    }

    ///////////////////////////////////////////////////////////////////////////////////////
    // Verbose print statements (only include when compiled in debug mode)
    #ifndef EMP_NDEBUG

    std::cout << "Evaluation summary: " << std::endl;
    for (auto world_ptr : worlds) {
      std::cout << "  " << world_ptr->GetName() << std::endl;
      std::cout << "    Pop size: " << world_ptr->GetNumOrgs() << std::endl;
      std::cout << "    Aggregate performance: " << world_ptr->GetAggregateTaskPerformance() << std::endl;
      std::cout << "    Criteria performances: ";
      for (size_t fun_i = 0; fun_i < world_ptr->GetNumSubTasks(); ++fun_i) {
        if (fun_i) std::cout << " ";
        std::cout << world_ptr->GetSubTaskPerformance(fun_i);
      }
      std::cout << std::endl;
    }

    #endif // EMP_NDEBUG
    ///////////////////////////////////////////////////////////////////////////////////////

    // Do selection: `selected` contains ids of selected populations.
    auto& selected = do_selection_fun();

    // Record results of evaluation?
    if (record_epoch) {
      world_evaluation_file->Update();
    }

    // For each selected world, extract a sample
    propagules.resize(config.NUM_POPS(), {});
    emp_assert(propagules.size()==selected.size());
    for (size_t i = 0; i < selected.size(); ++i) {
      // Sample propagules from each world!
      size_t selected_pop_id = selected[i];
      // Make sure selected pop is isn't extinct (in some weird edge case)
      // Note that we cannot be here if all populations are extinct. Spin until we pull a non-extinct pop.
      while (emp::Has(extinct_worlds, selected_pop_id)) {
        selected_pop_id = (selected_pop_id + 1) % worlds.size();
      }
      // Sample from the selected world to form the propagule.
      Sample(*worlds[selected_pop_id], propagules[i]);
    }

    // Reset worlds + inject propagules into them!
    const size_t propagule_offset = max_world_size*worlds.size(); // Propagules will have positions offset past all valid world positions

    const size_t transfer_time = (cur_epoch+1)*config.UPDATES_PER_EPOCH(); // cur_epoch+1 because this is at the end of an epoch (so after its updates have elapsed)
    if (config.TRACK_SYSTEMATICS()) {
      // To tie things together in the phylogeny tracking, make new organisms for each sampled organism in each propagule.
      // Add these organisms to the phylogeny, and use them as parents for injected organisms.
      size_t genome_counter = 0;
      for (size_t prop_i = 0; prop_i < propagules.size(); ++prop_i) {
        for (size_t gen_i = 0; gen_i < propagules[prop_i].size(); ++gen_i) {
          TransferOrg& transfer_org = propagules[prop_i][gen_i];
          systematics->SetNextParent(transfer_org.original_pos);
          systematics->AddOrg(*(transfer_org.org), {propagule_offset+genome_counter, 0}, (int)transfer_time);
          transfer_org.transfer_pos = propagule_offset+genome_counter;
          ++genome_counter;
        }
      }
    }

    for (size_t i = 0; i < config.NUM_POPS(); ++i) {
      auto& world = *(worlds[i]);
      world.DirectedDevoReset(); // Clear our the world.
      emp_assert(propagules[i].size(), "Propagule is empty.");
      SeedWithPropagule(world, propagules[i]); // NOTE - this will handle connecting injected organisms to transfer organisms in propagule
    }

    // Now, we need to remove each of the temporary propagule organisms from the systematics tracking.
    for (size_t prop_i = 0; prop_i < propagules.size(); ++prop_i) {
      for (size_t gen_i = 0; gen_i < propagules[prop_i].size(); ++gen_i) {
        TransferOrg& transfer_org = propagules[prop_i][gen_i];
        transfer_org.org.Delete(); // Delete transfer organism
        transfer_org.org = nullptr;
        if (config.TRACK_SYSTEMATICS()) systematics->RemoveOrgAfterRepro(transfer_org.transfer_pos, transfer_time);
      }
    }

    // Update the systematics manager
    if (config.TRACK_SYSTEMATICS()) {
      systematics->Update();
    }
  }
}

template <typename WORLD, typename ORG, typename MUTATOR, typename TASK, typename PERIPHERAL>
emp::map<typename emp::World<ORG>::genome_t, float> DirectedDevoExperiment<WORLD, ORG, MUTATOR, TASK, PERIPHERAL>::GetWorldFitnesses(emp::map<typename emp::World<ORG>::genome_t, size_t> unique_genomes, size_t world_id, size_t remove_cell) {
  //create world copy
  emp::Ptr<world_t> world_copy = emp::NewPtr<world_t>(
    config,
    #ifdef DIRDEVO_THREADING
    world_rngs[world_id],
    #else
    random,
    #endif
    "world_"+emp::to_string(world_id),
    world_id
  );

  //populate world copy with only unique genomes
  //position doesn't actually matter because the world is well-mixed
  for (auto& [genome, cell] : unique_genomes) {
    if (cell != remove_cell) {
      world_copy->InjectAt(genome, cell);
    }
  }

  //get fitnesses of genomes
  emp::map<typename emp::World<ORG>::genome_t, float> fitnesses = world_copy->RunFitnessTracking();

  //emp::assert unique_genomes.size() == fitnesses.size()
  world_copy.Delete();
  return fitnesses;
}

template <typename WORLD, typename ORG, typename MUTATOR, typename TASK, typename PERIPHERAL>
void DirectedDevoExperiment<WORLD, ORG, MUTATOR, TASK, PERIPHERAL>::RunStep() {
  // Advance each world by one step
  for (auto world_ptr : worlds) {
    world_ptr->RunStep();
  }
}

} // namespace dirdevo

#endif // #ifndef DIRECTED_DEVO_DIRECTED_DEVO_EXPERIMENT_HPP_INCLUDE