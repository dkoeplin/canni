#pragma once

#include <algorithm>
#include <functional>
#include <memory>
#include <string>

#include "nvl/data/Tensor.h"

/// Parses a CSV into a matrix of strings.
nvl::Tensor<2, std::string> parse_data(const std::string &filename, char delim = ',');