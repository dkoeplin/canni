#pragma once

#include <string>
#include <utility>
#include <vector>

#include "account/Accounts.h"
#include "invest/Day.h"

/// Provides some basic plotting utilities by creating a gnuplot external process.

struct PlotOptions {
    std::string title = "Balance Over Time";
    std::string x_title = "Date";
    std::string y_title = "Balance";
};

/// Shows a line plot of the total balance across all groups.
void line_plot(const Accounts &accounts, const PlotOptions &options = {});

/// Shows a stacked line plot based on the account groups.
void stacked_plot(const Accounts &accounts, const PlotOptions &options = {});

struct XYSeries {
    std::string title;
    std::vector<std::pair<double, double>> points;
    bool use_y2 = false;
};

struct XYPlotOptions {
    std::string title    = "Plot";
    std::string x_title  = "X";
    std::string y_title  = "Y";
    std::string y2_title = "";           // If non-empty, enables a second y-axis
    std::string x_format  = "$%.1s%c";   // gnuplot format string for x-axis
    std::string y_format  = "%.1s%c";    // gnuplot format string for left y-axis
    std::string y2_format = "%.1s%c";    // gnuplot format string for right y-axis
};

/// Shows an X/Y line plot with optional dual y-axes.
void xy_plot(const std::vector<XYSeries> &series, const XYPlotOptions &options = {});