#include "external/json.hpp"
#include <iostream>
#include <fstream>

std::unordered_map<std::string, std::tuple<std::string, bool, size_t>> known_algos =
  {
		   {"ST-Edit-None-Normal-Center_4-First-No-Matrix", {"Base", false, 4}},
		   {"ST-Edit-Undo-Normal-Center_4-First-No-Matrix", {"No Undo", false, 4}},
		   {"ST-Edit-Redundant-Normal-Center_4-First-No-Matrix", {"No Redundancy", false, 4}},
		   {"ST-Edit-Redundant-Skip-Center_4-First-No-Matrix", {"Skip Conversion", false, 4}},
		   {"ST-Edit-Redundant-Skip-Center_4-First-ARW-Matrix", {"ARW", false, 4}},
		   {"ST-Edit-Redundant-Skip-Center_4-Most-ARW-Matrix", {"ARW-Most", false, 4}},
		   {"ST-Edit-Redundant-Skip-Center_4-Most_Pruned-ARW-Matrix", {"ARW-Most Pruned", false, 4}},
		   {"ST-Edit-Redundant-Skip-Center_4-Single_Most-ARW-Matrix", {"ARW-Single", false, 4}},
		   {"ST-Edit-Redundant-Skip-Center_4-First-Basic-Matrix", {"Basic", false, 4}},
		   {"ST-Edit-Redundant-Skip-Center_4-Most-Basic-Matrix", {"Basic-Most", false, 4}},
		   {"ST-Edit-Redundant-Skip-Center_4-Most_Pruned-Basic-Matrix", {"Basic-Most Pruned", false, 4}},
		   {"ST-Edit-Redundant-Skip-Center_4-Single_Most-Basic-Matrix", {"Basic-Single", false, 4}}
  };

struct ExperimentKey {
  std::string graph;
  std::string algorithm;
  size_t permutation;
  bool mt;
  size_t threads;
  size_t l;
  size_t k;

  bool operator==(const ExperimentKey&other) const {
    return (graph == other.graph && algorithm == other.algorithm && permutation == other.permutation && mt == other.mt && threads == other.threads && l == other.l && k == other.k);
  }

};

static_assert(std::numeric_limits<double>::has_quiet_NaN, "NaN is required");

struct Experiment {
  std::string graph;
  std::string algorithm;
  size_t n, m, k, l, permutation;
  bool solved;
  bool mt;
  double time;
  size_t threads;
  size_t calls, extra_lbs, fallbacks, prunes, single, stolen, skipped;

  double scaling_factor_time, scaling_factor_calls;
  double speedup, efficiency;

  ExperimentKey get_key() const {
    return ExperimentKey {graph, algorithm, permutation, mt, threads, l, k};
  }

  Experiment() : n(0), m(0), k(0), l(0), permutation(0), solved(false), mt(false), time(0), threads(0), calls(0), extra_lbs(0), fallbacks(0), prunes(0), single(0), stolen(0), skipped(0), scaling_factor_time(std::numeric_limits<double>::quiet_NaN()), scaling_factor_calls(std::numeric_limits<double>::quiet_NaN()), speedup(std::numeric_limits<double>::quiet_NaN()), efficiency(std::numeric_limits<double>::quiet_NaN()) {}
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
      result ^= e.threads << 4;
      result ^= e.l << 5;
      result ^= e.k << 6;
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
        // sometimes, time is parsed as unsigned instead of float
        assert(last_key == "time");
        experiment.time = val;
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
    if (last_key == "time") {
      experiment.time = val;
    } else {
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
	  throw std::runtime_error("Error, duplicate experiment");
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
  if (argc != 3) {
    std::cout << "Exactly two parameters required: json_file.json output_file.csv" << std::endl;
    return 1;
  }

  std::ifstream input(argv[1]);

  std::unordered_map<ExperimentKey, Experiment> experiments;
  JSONCallback my_callback(experiments);

  nlohmann::json::sax_parse(input, &my_callback);

  for (auto &it : experiments) {
    if (it.second.k > 0) {
      ExperimentKey last_key = it.first;
      --last_key.k;

      auto last_it = experiments.find(last_key);
      if (last_it != experiments.end()) {
	it.second.scaling_factor_calls = static_cast<double>(it.second.calls) / last_it->second.calls;
	it.second.scaling_factor_time = it.second.time / last_it->second.time;
      }
    }

    ExperimentKey st_key = it.first;
    st_key.mt = false;
    st_key.threads = 1;

    auto st_it = experiments.find(st_key);
    if (st_it != experiments.end()) {
      it.second.speedup = st_it->second.time / it.second.time;
      it.second.efficiency = it.second.speedup / it.second.threads;
    }
  }

  std::ofstream of(argv[2]);

  of << "Graph,Algorithm,n,m,k,l,Permutation,Solved,MT,Time [s],Threads,Calls,Extra_lbs,Fallbacks,Prunes,Single,Skipped,Stolen,Scaling Factor Time,Scaling Factor Calls,Speedup,Efficiency" << std::endl;
  for (auto &it : experiments) {
    of << it.second.graph << "," << it.second.algorithm << "," << it.second.n << "," << it.second.m << "," << it.second.k << "," << it.second.l << "," << it.second.permutation << "," << (it.second.solved ? "True" : "False") << "," << (it.second.mt ? "True" : "False") << "," << it.second.time << "," << it.second.threads << "," << it.second.calls << "," << it.second.extra_lbs << "," << it.second.fallbacks << "," << it.second.prunes << "," << it.second.single << "," << it.second.skipped << "," << it.second.stolen << "," << it.second.scaling_factor_time << "," << it.second.scaling_factor_calls << "," << it.second.speedup << "," << it.second.efficiency << std::endl;
  }

  return 0;
}
