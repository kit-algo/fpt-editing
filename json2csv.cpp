#include "external/json.hpp"
#include <iostream>
#include <fstream>

#include <tlx/cmdline_parser.hpp>

std::unordered_map<std::string, std::tuple<std::string, bool, size_t>> known_algos =
  {
		   {"ST-Edit-None-Normal-Center_4-First-No-Matrix", {"FPT-No Optimization", false, 4}},
		   {"ST-Edit-Undo-Normal-Center_4-First-No-Matrix", {"FPT-No Undo", false, 4}},
		   {"ST-Edit-Redundant-Normal-Center_4-First-No-Matrix", {"FPT-No Redundancy", false, 4}},
		   {"ST-Edit-Redundant-Skip-Center_4-First-No-Matrix", {"FPT-Skip Conversion", false, 4}},
		   {"ST-Edit-Redundant-Skip-Center_4-First-ARW-Matrix", {"FPT-LS-F", false, 4}},
		   {"ST-Edit-Redundant-Skip-Center_4-Most-ARW-Matrix", {"FPT-LS-M", false, 4}},
		   {"ST-Edit-Redundant-Skip-Center_4-Most_Pruned-ARW-Matrix", {"FPT-LS-MP", false, 4}},
		   {"ST-Edit-Redundant-Skip-Center_4-Single_Most-ARW-Matrix", {"FPT-LS-Single", false, 4}},
		   {"ST-Edit-Redundant-Skip-Center_4-First-Basic-Matrix", {"FPT-G-F", false, 4}},
		   {"ST-Edit-Redundant-Skip-Center_4-First-Updated-Matrix", {"FPT-U-F", false, 4}},
		   {"ST-Edit-Redundant-Skip-Center_4-First-Min_Deg-Matrix", {"FPT-MD-F", false, 4}},
		   {"ST-Edit-Redundant-Skip-Center_4-Most-Basic-Matrix", {"FPT-G-M", false, 4}},
		   {"ST-Edit-Redundant-Skip-Center_4-Most-Updated-Matrix", {"FPT-U-M", false, 4}},
		   {"ST-Edit-Redundant-Skip-Center_4-Most-Min_Deg-Matrix", {"FPT-MD-M", false, 4}},
		   {"ST-Edit-Redundant-Skip-Center_4-Most_Pruned-Basic-Matrix", {"FPT-G-MP", false, 4}},
		   {"ST-Edit-Redundant-Skip-Center_4-Most_Pruned-Updated-Matrix", {"FPT-U-MP", false, 4}},
		   {"ST-Edit-Redundant-Skip-Center_4-Most_Pruned-Gurobi-Matrix", {"FPT-LP-MP", false, 4}},
		   {"ST-Edit-Redundant-Skip-Center_4-Most_Pruned-Min_Deg-Matrix", {"FPT-MD-MP", false, 4}},
		   {"ST-Edit-Redundant-Skip-Center_4-Single_Most-Basic-Matrix", {"FPT-G-Single", false, 4}},
		   {"MT-Edit-None-Normal-Center_4-First-No-Matrix", {"FPT-No Optimization", true, 4}},
		   {"MT-Edit-Undo-Normal-Center_4-First-No-Matrix", {"FPT-No Undo", true, 4}},
		   {"MT-Edit-Redundant-Normal-Center_4-First-No-Matrix", {"FPT-No Redundancy", true, 4}},
		   {"MT-Edit-Redundant-Skip-Center_4-First-No-Matrix", {"FPT-Skip Conversion", true, 4}},
		   {"MT-Edit-Redundant-Skip-Center_4-First-ARW-Matrix", {"FPT-LS-F", true, 4}},
		   {"MT-Edit-Redundant-Skip-Center_4-Most-ARW-Matrix", {"FPT-LS-M", true, 4}},
		   {"MT-Edit-Redundant-Skip-Center_4-Most_Pruned-ARW-Matrix", {"FPT-LS-MP", true, 4}},
		   {"MT-Edit-Redundant-Skip-Center_4-Single_Most-ARW-Matrix", {"FPT-LS-Single", true, 4}},
		   {"MT-Edit-Redundant-Skip-Center_4-First-Basic-Matrix", {"FPT-G-F", true, 4}},
		   {"MT-Edit-Redundant-Skip-Center_4-Most-Basic-Matrix", {"FPT-G-M", true, 4}},
		   {"MT-Edit-Redundant-Skip-Center_4-Most_Pruned-Basic-Matrix", {"FPT-G-MP", true, 4}},
		   {"MT-Edit-Redundant-Skip-Center_4-Single_Most-Basic-Matrix", {"FPT-G-Single", true, 4}},

		   {"Center_4-ST-basic-extended-constraints", {"ILP-Extended", false, 4}},
		   {"Center_4-ST-basic-sparse-extended-constraints", {"ILP-Sparse-Extended", false, 4}},
		   {"Center_4-ST-basic-single-extended-constraints", {"ILP-S-Extended", false, 4}},

		   {"Center_4-ST-basic", {"ILP-B", false, 4}},
		   {"Center_4-ST-basic-sparse", {"ILP-Sparse", false, 4}},
		   {"Center_4-ST-basic-single", {"ILP-S", false, 4}},

		   {"Center_4-ST-basic-lazy-3", {"ILP-Lazy", false, 4}},
		   {"Center_4-ST-basic-sparse-lazy-3", {"ILP-Sparse-Lazy", false, 4}},
		   {"Center_4-ST-basic-single-lazy-3", {"ILP-S-Lazy", false, 4}},

		   {"Center_4-ST-basic-extended-constraints-lazy-3", {"ILP-Extended-Lazy", false, 4}},
		   {"Center_4-ST-basic-sparse-extended-constraints-lazy-3", {"ILP-Sparse-Extended-Lazy", false, 4}},
		   {"Center_4-ST-basic-single-extended-constraints-lazy-3", {"ILP-S-Extended-Lazy", false, 4}},

                   {"Center_4-ST-iteratively-extended-constraints-lazy-3", {"Iteratively-Extended-Lazy", false, 4}},

		   {"Center_4-ST-basic-single-constraints-in-relaxation-single-c4", {"ILP-S-R-C4", false, 4}},
		   {"Center_4-MT-basic-single-constraints-in-relaxation-single-c4", {"ILP-S-R-C4", true, 4}},
		   {"Center_4-ST-basic-single-constraints-in-relaxation-heuristic-init-single-c4", {"ILP-S-R-C4-H", false, 4}},
		   {"Center_4-MT-basic-single-constraints-in-relaxation-heuristic-init-single-c4", {"ILP-S-R-C4-H", true, 4}},
		   {"Center_4-ST-basic-sparse-constraints-in-relaxation-single-c4", {"ILP-Sparse-R-C4", false, 4}},
		   {"Center_4-ST-basic-single-single-c4", {"ILP-S-C4", false, 4}},
		   {"Center_4-ST-basic-sparse-single-c4", {"ILP-Sparse-C4", false, 4}},
		   {"Center_4-ST-basic-single-constraints-in-relaxation", {"ILP-S-R", false, 4}},
		   {"Center_4-ST-basic-sparse-constraints-in-relaxation", {"ILP-Sparse-R", false, 4}},


		   {"Center_4-ST-basic-extended-constraints-heuristic", {"Heuristic-ILP-Extended", false, 4}},
		   {"Center_4-ST-basic-sparse-extended-constraints-heuristic", {"Heuristic-ILP-Sparse-Extended", false, 4}},
		   {"Center_4-ST-basic-single-extended-constraints-heuristic", {"Heuristic-ILP-S-Extended", false, 4}},

		   {"Center_4-ST-basic-heuristic", {"Heuristic-ILP-B", false, 4}},
		   {"Center_4-ST-basic-sparse-heuristic", {"Heuristic-ILP-Sparse", false, 4}},
		   {"Center_4-ST-basic-single-heuristic", {"Heuristic-ILP-S", false, 4}},

		   {"Center_4-ST-basic-lazy-3-heuristic", {"Heuristic-ILP-Lazy", false, 4}},
		   {"Center_4-ST-basic-sparse-lazy-3-heuristic", {"Heuristic-ILP-Sparse-Lazy", false, 4}},
		   {"Center_4-ST-basic-single-lazy-3-heuristic", {"Heuristic-ILP-S-Lazy", false, 4}},

		   {"Center_4-ST-basic-extended-constraints-lazy-3-heuristic", {"Heuristic-ILP-Extended-Lazy", false, 4}},
		   {"Center_4-ST-basic-sparse-extended-constraints-lazy-3-heuristic", {"Heuristic-ILP-Sparse-Extended-Lazy", false, 4}},
		   {"Center_4-ST-basic-single-extended-constraints-lazy-3-heuristic", {"Heuristic-ILP-S-Extended-Lazy", false, 4}},

                   {"Center_4-ST-iteratively-extended-constraints-lazy-3-heuristic", {"Heuristic-Iteratively-Extended-Lazy", false, 4}},

		   {"Center_4-ST-basic-single-constraints-in-relaxation-single-c4-heuristic", {"Heuristic-ILP-S-R-C4", false, 4}},
		   {"Center_4-MT-basic-single-constraints-in-relaxation-single-c4-heuristic", {"Heuristic-ILP-S-R-C4", true, 4}},
		   {"Center_4-ST-basic-single-constraints-in-relaxation-heuristic-init-single-c4-heuristic", {"Heuristic-ILP-S-R-C4-H", false, 4}},
		   {"Center_4-MT-basic-single-constraints-in-relaxation-heuristic-init-single-c4-heuristic", {"Heuristic-ILP-S-R-C4-H", true, 4}},
		   {"Center_4-ST-basic-sparse-constraints-in-relaxation-single-c4-heuristic", {"Heuristic-ILP-Sparse-R-C4", false, 4}},
		   {"Center_4-ST-basic-single-single-c4-heuristic", {"Heuristic-ILP-S-C4", false, 4}},
		   {"Center_4-ST-basic-sparse-single-c4-heuristic", {"Heuristic-ILP-Sparse-C4", false, 4}},
		   {"Center_4-ST-basic-single-constraints-in-relaxation-heuristic", {"Heuristic-ILP-S-R", false, 4}},
		   {"Center_4-ST-basic-sparse-constraints-in-relaxation-heuristic", {"Heuristic-ILP-Sparse-R", false, 4}},

		   {"Center_4-MT-basic-extended-constraints", {"ILP-Extended", true, 4}},
		   {"Center_4-MT-basic-sparse-extended-constraints", {"ILP-Sparse-Extended", true, 4}},
		   {"Center_4-MT-basic-single-extended-constraints", {"ILP-S-Extended", true, 4}},

		   {"Center_4-MT-basic", {"ILP", true, 4}},
		   {"Center_4-MT-basic-sparse", {"ILP-Sparse", true, 4}},
		   {"Center_4-MT-basic-single", {"ILP-S", true, 4}},

		   {"Center_4-MT-basic-lazy-3", {"ILP-Lazy", true, 4}},
		   {"Center_4-MT-basic-sparse-lazy-3", {"ILP-Sparse-Lazy", true, 4}},
		   {"Center_4-MT-basic-single-lazy-3", {"ILP-S-Lazy", true, 4}},

		   {"Center_4-MT-basic-extended-constraints-lazy-3", {"ILP-Extended-Lazy", true, 4}},
		   {"Center_4-MT-basic-sparse-extended-constraints-lazy-3", {"ILP-Sparse-Extended-Lazy", true, 4}},
		   {"Center_4-MT-basic-single-extended-constraints-lazy-3", {"ILP-S-Extended-Lazy", true, 4}},

                   {"Center_4-MT-iteratively-extended-constraints-lazy-3", {"Iteratively-Extended-Lazy", true, 4}},


		   {"Center_4-MT-basic-extended-constraints-heuristic", {"Heuristic-ILP-Extended", true, 4}},
		   {"Center_4-MT-basic-extended-constraints-sparse-heuristic", {"Heuristic-ILP-Sparse-Extended", true, 4}},
		   {"Center_4-MT-basic-extended-constraints-single-heuristic", {"Heuristic-ILP-S-Extended", true, 4}},

		   {"Center_4-MT-basic-heuristic", {"Heuristic-ILP", true, 4}},
		   {"Center_4-MT-basic-sparse-heuristic", {"Heuristic-ILP-Sparse", true, 4}},
		   {"Center_4-MT-basic-single-heuristic", {"Heuristic-ILP-S", true, 4}},

		   {"Center_4-MT-basic-lazy-3-heuristic", {"Heuristic-ILP-Lazy", true, 4}},
		   {"Center_4-MT-basic-sparse-lazy-3-heuristic", {"Heuristic-ILP-Sparse-Lazy", true, 4}},
		   {"Center_4-MT-basic-single-lazy-3-heuristic", {"Heuristic-ILP-S-Lazy", true, 4}},

		   {"Center_4-MT-basic-extended-constraints-lazy-3-heuristic", {"Heuristic-ILP-Extended-Lazy", true, 4}},
		   {"Center_4-MT-basic-sparse-extended-constraints-lazy-3-heuristic", {"Heuristic-ILP-Sparse-Extended-Lazy", true, 4}},
		   {"Center_4-MT-basic-single-extended-constraints-lazy-3-heuristic", {"Heuristic-ILP-S-Extended-Lazy", true, 4}},

                   {"Center_4-MT-iteratively-extended-constraints-lazy-3-heuristic", {"Heuristic-Iteratively-Extended-Lazy", true, 4}},
  };

struct ExperimentKey {
  std::string graph;
  std::string algorithm;
  size_t permutation;
  bool mt;
  bool all_solutions;
  size_t threads;
  size_t l;
  size_t k;

  bool operator==(const ExperimentKey&other) const {
    return (graph == other.graph && algorithm == other.algorithm && permutation == other.permutation && mt == other.mt && all_solutions == other.all_solutions && threads == other.threads && l == other.l && k == other.k);
  }

};

static_assert(std::numeric_limits<double>::has_quiet_NaN, "NaN is required");

struct Experiment {
  std::string graph;
  std::string algorithm;
  size_t n, m, k, l, permutation;
  bool all_solutions;
  bool solved;
  bool mt;
  double time, time_initialization, total_time;
  size_t threads;
  size_t calls, extra_lbs, fallbacks, prunes, single, stolen, skipped;
  size_t solutions;

  double scaling_factor_time, scaling_factor_calls;
  double speedup, efficiency;

  ExperimentKey get_key() const {
    return ExperimentKey {graph, algorithm, permutation, mt, all_solutions, threads, l, k};
  }

  Experiment() : n(0), m(0), k(0), l(0), permutation(0), all_solutions(false), solved(false), mt(false), time(0), time_initialization(0), total_time(0), threads(0), calls(0), extra_lbs(0), fallbacks(0), prunes(0), single(0), stolen(0), skipped(0), solutions(0), scaling_factor_time(std::numeric_limits<double>::quiet_NaN()), scaling_factor_calls(std::numeric_limits<double>::quiet_NaN()), speedup(std::numeric_limits<double>::quiet_NaN()), efficiency(std::numeric_limits<double>::quiet_NaN()) {}
};


namespace std {
  template <> struct hash<ExperimentKey> {
    using argument_type = ExperimentKey;
    using result_type = std::size_t;
    result_type operator()(argument_type const&e) const noexcept {
      size_t result = hash<string>()(e.graph);
      result ^= hash<string>()(e.algorithm) << 1;
      result ^= e.permutation << 2;
      result ^= static_cast<size_t>(e.mt) << 3;
      result ^= static_cast<size_t>(e.all_solutions) << 4;
      result ^= e.threads << 5;
      result ^= e.l << 6;
      result ^= e.k << 7;
      return result;
    }
  };
}


class JSONCallback : public nlohmann::json_sax<nlohmann::json> {
private:
  size_t num_graphs;
  size_t object_depth;
  std::string last_key;
  Experiment experiment;
  bool found_error;
  std::unordered_map<ExperimentKey, Experiment> &experiments;
public:

  JSONCallback(std::unordered_map<ExperimentKey, Experiment> &experiments) : num_graphs(0), object_depth(0), found_error(false), experiments(experiments) {}

  // called when null is parsed
  bool null() override {
    throw std::runtime_error("unexpected integer value for key " + last_key);
    return true;
  }

// called when a boolean is parsed; value is passed
  bool boolean(bool) override {
    throw std::runtime_error("unexpected integer value for key " + last_key);
    return true;
  }

// called when a signed or unsigned integer number is parsed; value is passed
  bool number_integer(number_integer_t) override {
      throw std::runtime_error("unexpected integer value for key " + last_key);
      return true;
  }

  bool number_unsigned(number_unsigned_t val) override {
    switch (last_key[0]) {
    case 'p':
      switch (last_key[1]) {
      case 'e': // permutation
        assert(last_key == "permutation");
        experiment.permutation = val;
        break;
      case 'r': // prunes
        assert(last_key == "prunes");
        experiment.prunes += val;
        break;
      default:
        throw std::runtime_error("unexpected unsigned value for key " + last_key);
      }
      break;
    case 'n': // n
      assert(last_key == "n");
      experiment.n = val;
      break;
    case 'm': // m
      assert(last_key == "m");
      experiment.m = val;
      break;
    case 't':
      switch (last_key[1]) {
      case 'h': // threads
        assert(last_key == "threads");
        experiment.threads = val;
        break;
      case 'i': // time
        if (last_key.size() == 4) // time
        {
            // sometimes, time is parsed as unsigned instead of float
            assert(last_key == "time");
            experiment.time = val;
        }
        else if (last_key.size() == 19)
        {
            // sometimes, time is parsed as unsigned instead of float
            assert(last_key == "time_initialization");
            experiment.time_initialization = val;
        }
        else
        {
            throw std::runtime_error("unexpected unsigned value for key " + last_key);
        }
        break;
      case 'o': // total_time
        assert(last_key == "total_time");
        experiment.total_time = val;
        break;
      default:
        throw std::runtime_error("unexpected unsigned value for key " + last_key);
      }
      break;
    case 'k': // k
      assert(last_key == "k");
      experiment.k = val;
      break;
    case 'c': // calls
      assert(last_key == "calls");
      experiment.calls += val;
      break;
    case 'e': // extra_lbs
      assert(last_key == "extra_lbs");
      experiment.extra_lbs += val;
      break;
    case 'f': // fallbacks
      assert(last_key == "fallbacks");
      experiment.fallbacks += val;
      break;
    case 's':
      switch (last_key[1]) {
      case 'i': // single
        assert(last_key == "single");
        experiment.single += val;
        break;
      case 't': // stolen
        assert(last_key == "stolen");
        experiment.stolen += val;
        break;
      case 'k': // skipped
        assert(last_key == "skipped");
        experiment.skipped += val;
        break;
      case 'o': // solutions
        assert(last_key == "solutions");
        experiment.solutions += val;
        break;
      default:
        throw std::runtime_error("unexpected unsigned value for key " + last_key);
      }
      break;
    default:
      throw std::runtime_error("unexpected unsigned value for key " + last_key);
    }
    return true;
  }

// called when a floating-point number is parsed; value and original string is passed
  bool number_float(number_float_t val, const string_t&) override {
    switch (last_key[0])
    {
    case 't':
      switch (last_key[1]) {
      case 'i': // time
        if (last_key.size() == 4) // time
        {
            // sometimes, time is parsed as unsigned instead of float
            assert(last_key == "time");
            experiment.time = val;
        }
        else if (last_key.size() == 19)
        {
            // sometimes, time is parsed as unsigned instead of float
            assert(last_key == "time_initialization");
            experiment.time_initialization = val;
        }
        else
        {
            throw std::runtime_error("unexpected float value for key " + last_key);
        }
        break;
      case 'o': // total_time
        assert(last_key == "total_time");
        experiment.total_time = val;
        break;
      default:
        throw std::runtime_error("unexpected float value for key " + last_key);
      }
      break;
    default:
      throw std::runtime_error("unexpected float value for key " + last_key);
    }
    return true;
  }

// called when a string is parsed; value is passed and can be safely moved away
  bool string(string_t& val) override {
    if (last_key == "graph") {
      auto const pos_slash = val.find_last_of('/');
      auto const pos_dot = val.find_last_of('.');
      if (pos_slash == std::string::npos) {
	if (pos_dot == std::string::npos) {
	  experiment.graph = std::move(val);
	} else {
	  experiment.graph = val.substr(0, pos_dot);
	}
      } else {
	if (pos_dot == std::string::npos) {
	  experiment.graph = val.substr(pos_slash + 1);
	} else {
	  experiment.graph = val.substr(pos_slash + 1, pos_dot - pos_slash - 1);
	}
      }
      const std::string metis_suffix = ".metis";
      if (experiment.graph.size() > metis_suffix.size() && experiment.graph.substr(experiment.graph.size() - metis_suffix.size()) == metis_suffix) {
        experiment.graph = experiment.graph.substr(0, experiment.graph.size() - metis_suffix.size());
      }
    } else if (last_key == "algo") {
      auto algo_it = known_algos.find(val);
      if (algo_it == known_algos.end()) {
	throw std::runtime_error("Unknown algorithm "  + val);
      }
      std::tie(experiment.algorithm, experiment.mt, experiment.l) = algo_it->second;
    } else if (last_key == "solved") {
      experiment.solved = (val == "true");
    } else if (last_key == "type") {
      // nothing to do
    } else if (last_key == "error") {
      if (val != "Timeout") {
	throw std::runtime_error("unexpected error " + val);
      }
      found_error = true;
    } else if (last_key == "all_solutions") {
      experiment.all_solutions = (val == "true");
    } else {
      throw std::runtime_error("unexpcted string value " + val + " for key " + last_key);
    }
    return true;
  }

// called when an object or array begins or ends, resp. The number of elements is passed (or -1 if not known)
  bool start_object(std::size_t) override {
    ++object_depth;
    return true;
  }

  bool end_object() override {
    --object_depth;

    if (object_depth == 0) {
      if (!found_error) {
	const ExperimentKey key = experiment.get_key();
	auto it = experiments.find(key);
	if (it != experiments.end()) {
	  experiments.erase(it);
	  std::cout << "Warning: duplicate experiment deleted graph: " << experiment.graph << " algorithm: " << experiment.algorithm << " threads: " << experiment.threads << " k: " << experiment.k << " permutation: " << experiment.permutation << std::endl;
	  //throw std::runtime_error("Error, duplicate experiment");
	}

	experiments.emplace(std::move(key), std::move(experiment));
      }
      experiment = Experiment();
      found_error = false;
    }
    return true;
  }

  bool start_array(std::size_t /*elements*/) override {return true;}
  bool end_array() override { return true; }
// called when an object key is parsed; value is passed and can be safely moved away
  bool key(string_t& val) override {
    if (val == "graph") {
      ++num_graphs;
      if (num_graphs % 1000 == 0) std::cout << "read " << num_graphs << " graphs" << std::endl;
    }

    last_key = std::move(val);
    return true;
  }

// called when a parse error occurs; byte position, the last token, and an exception is passed
  bool parse_error(std::size_t /*position*/, const std::string& /*last_token*/, const nlohmann::detail::exception& ex) override {
    throw ex;
  }

};

int main(int argc, char *argv[]) {
  std::string output_filename;
  std::vector<std::string> input_filenames;
  size_t time_limit = 1000;

  tlx::CmdlineParser cp;
  cp.set_description("Convert the given json file into a CSV file");
  cp.add_size_t('t', "time-limit", time_limit, "The time limit, results whose (rounded down) running time exceeds that time limit are discarded");
  cp.add_param_string("csv_file", output_filename, "The output csv filename");
  cp.add_param_stringlist("json_file", input_filenames, "The input json filenames");
  if (!cp.process(argc, argv)) {
    return 1;
  }

  std::unordered_map<ExperimentKey, Experiment> experiments;
  JSONCallback my_callback(experiments);

  for (const std::string &input_filename : input_filenames) {
    std::ifstream input(input_filename);
    nlohmann::json::sax_parse(input, &my_callback);
  }

  for (auto it = experiments.begin(); it != experiments.end(); ) {
    if (static_cast<size_t>(it->second.total_time) > time_limit) {
      std::cout << "Note: deleting (" << (it->second.solved ? "" : "un") << "solved) experiment that exceeded running time: " << it->second.graph << " algorithm: " << it->second.algorithm << " threads: " << it->second.threads << " k: " << it->second.k << " permutation: " << it->second.permutation << " total time: " << it->second.total_time << std::endl;
      it = experiments.erase(it);
    } else {
      ++it;
    }
  }

  for (auto &it : experiments) {
    if (it.second.k > 0) {
      ExperimentKey last_key = it.first;
      --last_key.k;

      auto last_it = experiments.find(last_key);
      if (last_it != experiments.end()) {
	it.second.scaling_factor_calls = static_cast<double>(it.second.calls) / last_it->second.calls;
	it.second.scaling_factor_time = it.second.total_time / last_it->second.total_time;
      }
    }

    ExperimentKey st_key = it.first;
    st_key.mt = false;
    st_key.threads = 1;

    auto st_it = experiments.find(st_key);
    if (st_it != experiments.end()) {
      it.second.speedup = st_it->second.total_time / it.second.total_time;
      it.second.efficiency = it.second.speedup / it.second.threads;
    }
  }

  std::ofstream of(output_filename);

  of << "Graph,Algorithm,n,m,k,l,Permutation,Solved,MT,All Solutions,Time [s],Total Time [s],Time Initialization [s],Threads,Calls,Extra_lbs,Fallbacks,Prunes,Single,Skipped,Stolen,Solutions,Scaling Factor Time,Scaling Factor Calls,Speedup,Efficiency" << std::endl;
  for (auto &it : experiments) {
    of << it.second.graph << "," << it.second.algorithm << "," << it.second.n << "," << it.second.m << "," << it.second.k << "," << it.second.l << "," << it.second.permutation << "," << (it.second.solved ? "True" : "False") << "," << (it.second.mt ? "True" : "False") << "," << (it.second.all_solutions ? "True" : "False") << "," << it.second.time << "," << it.second.total_time << "," << it.second.time_initialization << "," << it.second.threads << "," << it.second.calls << "," << it.second.extra_lbs << "," << it.second.fallbacks << "," << it.second.prunes << "," << it.second.single << "," << it.second.skipped << "," << it.second.stolen << "," << it.second.solutions << "," << it.second.scaling_factor_time << "," << it.second.scaling_factor_calls << "," << it.second.speedup << "," << it.second.efficiency << std::endl;
  }

  return 0;
}
