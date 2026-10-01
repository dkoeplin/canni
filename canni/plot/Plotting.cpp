#include "Plotting.h"

#include <algorithm>
#include <functional>
#include <memory>
#include <_stdio.h>

#include <ranges>

#include "canni/Macros.h"

namespace canni {

namespace detail {

std::string axis_name(Axis axis) {
    switch (axis) {
    case kX: return "x";
    case kY: return "y";
    case kY2: return "y2";
    }
    UNREACHABLE;
}

FILE *setup_plot(const XYPlotOptions &options, const bool has_y2, const bool stacked) {
    FILE *plot = popen("gnuplot -persistent", "w");
    if (!plot) {
        std::cerr << "Failed to open gnuplot pipe" << std::endl;
        return nullptr;
    }
    fprintf(plot, "set title '%s'\n", options.title.c_str());
    fprintf(plot, "set xlabel '%s'\n", options.x_title.c_str());
    fprintf(plot, "set ylabel '%s'\n", options.y_title.c_str());
    fprintf(plot, "set grid\n");
    fprintf(plot, "set term qt font \"Arial\"\n");
    fprintf(plot, "set timefmt '%%Y-%%m-%%d'\n");
    fprintf(plot, "set ytics nomirror\n");
    if (has_y2) {
        fprintf(plot, "set y2label '%s'\n", options.y2_title.c_str());
        fprintf(plot, "set y2tics\n");
    }
    if (stacked) {
        fprintf(plot, "set style fill transparent solid 1.0\n");
    }
    return plot;
}

void setup_numeric_axis(FILE* plot, const Axis axis) {
    fprintf(plot, "set format %s '%%.2f'\n", axis_name(axis).c_str());
}

void AxisFormat<Day>::setup_axis(FILE* plot, Axis axis) {
    fprintf(plot, "set %sdata time\n", axis_name(axis).c_str());
    fprintf(plot, "set format %s '%%Y'\n", axis_name(axis).c_str());
}

void AxisFormat<USD>::setup_axis(FILE* plot, Axis axis) {
    fprintf(plot, "set format %s '$%%.1s%%c'\n", axis_name(axis).c_str());
}

} // namespace detail

} // namespace canni
