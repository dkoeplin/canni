#pragma once

#include <string>
#include <vector>

struct XYSeries {
    std::string name;
    std::vector<std::pair<std::string, double>> points;
    bool use_y2 = false;
};
