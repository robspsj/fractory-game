#include "item_table.hpp"

#include <fstream>
#include <set>
#include <sstream>
#include <vector>

namespace {

// Split one CSV row on commas, trimming spaces/tabs around each field.
std::vector<std::string> splitRow(const std::string &line) {
  std::vector<std::string> out;
  std::stringstream ss(line);
  std::string field;
  while (std::getline(ss, field, ',')) {
    const size_t a = field.find_first_not_of(" \t\r");
    const size_t b = field.find_last_not_of(" \t\r");
    out.push_back(a == std::string::npos ? "" : field.substr(a, b - a + 1));
  }
  return out;
}

}  // namespace

int naturalItemCountFromCsv(const std::string &csvText, int fallback) {
  std::set<int> natural;
  std::stringstream in(csvText);
  std::string line;
  while (std::getline(in, line)) {
    if (line.empty() || line[0] == '#') continue;
    const std::vector<std::string> f = splitRow(line);
    if (f.size() < 9) continue;   // no `natural` column: not declared here
    if (f[8] != "1" && f[8] != "true" && f[8] != "yes") continue;
    try {
      natural.insert(std::stoi(f[0]));
    } catch (...) {
      continue;
    }
  }
  // Only the contiguous prefix 0..n-1 counts: generation rolls id % numItems.
  int n = 0;
  while (natural.count(n)) n++;
  return n > 0 ? n : fallback;
}

int naturalItemCountFromFile(const std::string &path, int fallback) {
  if (path.empty()) return fallback;
  std::ifstream file(path);
  if (!file) return fallback;
  std::stringstream buf;
  buf << file.rdbuf();
  return naturalItemCountFromCsv(buf.str(), fallback);
}
