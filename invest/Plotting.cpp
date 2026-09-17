#include "Plotting.h"

#include <algorithm>
#include <functional>
#include <memory>
#include <_stdio.h>

#include <ranges>

#include "nvl/data/List.h"

#if 0
void stacked_plot(const Accounts &accounts, const XYPlotOptions &options) {
    FILE *gnuplot = popen("gnuplot -persistent", "w");
    if (!gnuplot) {
        std::cerr << "Failed to open gnuplot pipe" << std::endl;
    }
    fprintf(gnuplot, "set title '%s'\n", options.title.c_str());
    fprintf(gnuplot, "set xlabel '%s'\n", options.x_title.c_str());
    fprintf(gnuplot, "set ylabel '%s'\n", options.y_title.c_str());
    fprintf(gnuplot, "set xdata time\n");
    fprintf(gnuplot, "set timefmt '%%Y-%%m-%%d'\n");
    fprintf(gnuplot, "set format x '%%Y'\n");
    fprintf(gnuplot, "set format y '$%%.1s%%c'\n");
    fprintf(gnuplot, "set yrange [0:*]\n");
    fprintf(gnuplot, "set grid\n");
    fprintf(gnuplot, "set term qt font \"Arial\"\n");

    // One `plot '-'` block per group
    fprintf(gnuplot, "plot ");
    const auto last = std::prev(accounts.rend());
    for (auto group = accounts.rbegin(); group != accounts.rend(); ++group) {
        const auto delim = group != last ? ", \\\n     " : "\n";
        const auto group_name = std::string(*group);
    }

    const auto totals = accounts.totals();
    for (const auto group : std::views::reverse(accounts)) {
        for (const auto &entry : totals) {
            const USD cumulative = entry.cumulative(group);
            fprintf(gnuplot, "%s %.2f\n", entry.day.to_string("%Y-%m-%d").c_str(), cumulative.f64());
        }
        fprintf(gnuplot, "e\n");
    }
    pclose(gnuplot);
}
#endif

void xy_plot(const std::vector<XYSeries> &series, const XYPlotOptions &options) {
    FILE *gnuplot = popen("gnuplot -persistent", "w");
    if (!gnuplot) {
        std::cerr << "Failed to open gnuplot pipe" << std::endl;
        return;
    }
    fprintf(gnuplot, "set title '%s'\n", options.title.c_str());
    fprintf(gnuplot, "set xlabel '%s'\n", options.x_title.c_str());
    fprintf(gnuplot, "set ylabel '%s'\n", options.y_title.c_str());
    if (options.x_format == "TIME") {
        fprintf(gnuplot, "set xdata time\n");
        fprintf(gnuplot, "set timefmt '%%Y-%%m-%%d'\n");
        fprintf(gnuplot, "set format x '%%Y'\n");
    } else {
        fprintf(gnuplot, "set format x '%s'\n", options.x_format.c_str());
    }
    fprintf(gnuplot, "set format y '%s'\n", options.y_format.c_str());
    if (std::ranges::any_of(series, [](const XYSeries &series){ return series.use_y2; })) {
        fprintf(gnuplot, "set y2label '%s'\n", options.y2_title.c_str());
        fprintf(gnuplot, "set format y2 '%s'\n", options.y2_format.c_str());
        fprintf(gnuplot, "set ytics nomirror\n");
        fprintf(gnuplot, "set y2tics\n");
    }
    if (options.stacked) {
        fprintf(gnuplot, "set style fill transparent solid 1.0\n");
    }
    fprintf(gnuplot, "set grid\n");
    fprintf(gnuplot, "set term qt font \"Arial\"\n");

    fprintf(gnuplot, "plot ");
    const auto last = std::prev(series.end());
    for (auto s = series.begin(); s != series.end(); ++s) {
        const char *axes = s->use_y2 ? "axes x1y2 " : "";
        const char *delim = s != last ? ", \\\n     " : "\n";
        if (options.stacked) {
            fprintf(gnuplot, "'-' using 1:2 %swith filledcurves x1 title '%s'%s", axes, s->name.c_str(), delim);
        } else {
            fprintf(gnuplot, "'-' using 1:2 %swith lines lw 2 title '%s'%s", axes, s->name.c_str(), delim);
        }
    }
    for (U64 i = 0; i < series.size(); ++i) {
        const auto &s = series[i];
        for (U64 j = 0; j < s.points.size(); ++j) {
            const auto &x = s.points[j].first;
            F64 y = s.points[j].second;
            if (options.stacked) {
                for (U64 ii = i + 1; ii < series.size(); ++ii)
                    y += series[ii].points[j].second;
            }
            fprintf(gnuplot, "%s %.2f\n", x.c_str(), y);
        }
        fprintf(gnuplot, "e\n");
    }
    pclose(gnuplot);
}
