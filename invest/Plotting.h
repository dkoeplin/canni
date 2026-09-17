#pragma once

#include <string>
#include <vector>

#include "invest/XYSeries.h"

struct XYPlotOptions {
    std::string title    = "Plot";
    std::string x_title  = "X";
    std::string y_title  = "Y";
    std::string y2_title = "Y2";         // If non-empty, enables a second y-axis
    std::string x_format  = "$%.1s%c";   // gnuplot format string for x-axis
    std::string y_format  = "%.1s%c";    // gnuplot format string for left y-axis
    std::string y2_format = "%.1s%c";    // gnuplot format string for right y-axis
    bool stacked = false;
};

/// Shows an X/Y line plot with optional dual y-axes.
void xy_plot(const std::vector<XYSeries> &series, const XYPlotOptions &options = {});
