#pragma once

#include <string>
#include <vector>

namespace canni {

template <typename X, typename Y>
struct Series {
    std::string name;
    std::vector<std::pair<X, Y>> points;
    bool use_y2 = false;
};

} // namespace canni
