#pragma once

#include <string>
#include <vector>

namespace canni {

/// Parses a CSV into a matrix of strings.
std::vector<std::vector<std::string>> parse_data(const std::string &filename, char delim = ',');

} // namespace canni