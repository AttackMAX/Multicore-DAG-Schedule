#ifndef UTILS_JSON_UTILS_H_
#define UTILS_JSON_UTILS_H_

#include <cstdint>
#include <ostream>
#include <string>

namespace utils {

inline void JsonString(std::ostream& os, const std::string& s) {
  os << '"';
  for (char c : s) {
    switch (c) {
      case '"':  os << "\\\""; break;
      case '\\': os << "\\\\"; break;
      case '\n': os << "\\n";  break;
      case '\r': os << "\\r";  break;
      case '\t': os << "\\t";  break;
      default:   os << c;
    }
  }
  os << '"';
}

class Comma {
 public:
  void Write(std::ostream& os) {
    if (!first_) os << ", ";
    first_ = false;
  }
  void Reset() { first_ = true; }

 private:
  bool first_ = true;
};

inline void WriteKey(std::ostream& os, const char* key) {
  JsonString(os, key);
  os << ": ";
}

inline void WriteInt(std::ostream& os, const char* key, int val) {
  WriteKey(os, key);
  os << val;
}

inline void WriteInt64(std::ostream& os, const char* key, std::int64_t val) {
  WriteKey(os, key);
  os << val;
}

inline void WriteDouble(std::ostream& os, const char* key, double val) {
  WriteKey(os, key);
  os << val;
}

inline void WriteString(std::ostream& os, const char* key,
                         const std::string& val) {
  WriteKey(os, key);
  JsonString(os, val);
}

inline void WriteBool(std::ostream& os, const char* key, bool val) {
  WriteKey(os, key);
  os << (val ? "true" : "false");
}

}  // namespace utils

#endif
