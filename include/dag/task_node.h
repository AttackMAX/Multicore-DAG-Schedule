#ifndef DAG_TASK_NODE_H_
#define DAG_TASK_NODE_H_

#include <cstdint>
#include <vector>

namespace dag {

struct TaskNode {
  int id = -1;
  std::int64_t duration = 0;
  std::vector<int> outgoing_edges;
};

}

#endif
