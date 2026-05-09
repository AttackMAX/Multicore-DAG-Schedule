#ifndef IO_STG_READER_H_
#define IO_STG_READER_H_

#include <string>

#include "dag/dag_graph.h"

namespace io {

class StgReader {
 public:
  // Reads an STG file. If include_dummy is false (default), dummy source
  // and sink nodes are stripped and their incident edges are removed.
  static dag::DagGraph Read(const std::string& path, bool include_dummy = false);
};

}  // namespace io

#endif
