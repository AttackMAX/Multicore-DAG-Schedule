#include "io/stg_reader.h"

#include <cctype>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace io {

dag::DagGraph StgReader::Read(const std::string& path, bool include_dummy) {
  std::ifstream file(path);
  if (!file.is_open()) {
    return dag::DagGraph();
  }

  int real_task_count = 0;
  bool header_done = false;
  std::vector<int> node_ids;
  std::vector<std::int64_t> durations;
  std::vector<std::vector<int>> predecessors;

  std::string line;
  while (std::getline(file, line)) {
    // Trim leading whitespace
    size_t start = 0;
    while (start < line.size() && std::isspace(line[start])) ++start;
    if (start >= line.size()) continue;
    // Skip comments
    if (line[start] == '#') continue;

    // Tokenize into integers
    std::istringstream iss(line);
    std::vector<int> tokens;
    int val;
    while (iss >> val) {
      tokens.push_back(val);
    }
    if (tokens.empty()) continue;

    if (!header_done) {
      // Header line holds the real task count.
      real_task_count = tokens[0];
      header_done = true;
      continue;
    }

    // Node line: id, duration, num_preds, [pred1, pred2, ...]
    // Minimum 3 tokens (id, duration, 0 preds for dummy source).
    if (tokens.size() < 3) continue;
    int node_id = tokens[0];
    std::int64_t duration = tokens[1];
    int num_preds = tokens[2];

    if (!include_dummy) {
      // Dummy source is id 0; dummy sink is id real_task_count + 1.
      if (node_id == 0 || node_id == real_task_count + 1) continue;
    }

    node_ids.push_back(node_id);
    durations.push_back(duration);

    std::vector<int> preds;
    for (int i = 0; i < num_preds && i + 3 < static_cast<int>(tokens.size()); ++i) {
      int pred = tokens[3 + i];
      if (include_dummy) {
        preds.push_back(pred);
      } else if (pred != 0) {
        // Skip edges from the dummy source
        preds.push_back(pred);
      }
    }
    predecessors.push_back(preds);
  }

  if (node_ids.empty()) return dag::DagGraph();

  dag::DagGraph graph;
  for (std::size_t i = 0; i < node_ids.size(); ++i) {
    graph.AddNode(node_ids[i], durations[i]);
  }
  for (std::size_t i = 0; i < node_ids.size(); ++i) {
    for (int pred : predecessors[i]) {
      graph.AddEdge(pred, node_ids[i]);
    }
  }

  return graph;
}

}  // namespace io
