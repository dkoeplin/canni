#include "Plotting.h"

#include <_stdio.h>

#include "nvl/data/List.h"

void line_plot(const Accounts &accounts, const PlotOptions &options) {
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
    fprintf(gnuplot, "set format y '$%%.1s%%c'\n");  // Magnitude with 1 decimal point
    fprintf(gnuplot, "set yrange [0:*]\n");
    fprintf(gnuplot, "set grid\n");
    fprintf(gnuplot, "set term qt font \"Arial\"\n");
    fprintf(gnuplot, "plot '-' using 1:2 with lines lw 2 title '%s'\n", options.title.c_str());

    for (const auto &day : accounts.dates()) {
        const auto &amt = accounts.total(day);
        fprintf(gnuplot, "%s %.2f\n", day.to_string("%Y-%m-%d").c_str(), amt.f64());
    }
    fprintf(gnuplot, "e\n");
    pclose(gnuplot);
}

void stacked_plot(const Accounts &accounts, const PlotOptions &options) {
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
    fprintf(gnuplot, "set style fill transparent solid 1.0\n");
    fprintf(gnuplot, "set term qt font \"Arial\"\n");

    // One `plot '-'` block per group
    fprintf(gnuplot, "plot ");
    const auto last = std::prev(accounts.rend());
    for (auto group = accounts.rbegin(); group != accounts.rend(); ++group) {
        const auto delim = group != last ? ", \\\n     " : "\n";
        const auto group_name = std::string(*group);
        fprintf(gnuplot, "'-' using 1:2 with filledcurves x1 title '%s'%s", group_name.c_str(), delim);
    }

    for (auto group = accounts.rbegin(); group != accounts.rend(); ++group) {
        for (const Day &day : accounts.dates()) {
            const USD cumulative = accounts.cumulative(*group, day);
            fprintf(gnuplot, "%s %.2f\n", day.to_string("%Y-%m-%d").c_str(), cumulative.f64());
        }
        fprintf(gnuplot, "e\n");
    }
    pclose(gnuplot);
}

void xy_plot(const std::vector<XYSeries> &series, const XYPlotOptions &options) {
    FILE *gnuplot = popen("gnuplot -persistent", "w");
    if (!gnuplot) {
        std::cerr << "Failed to open gnuplot pipe" << std::endl;
        return;
    }
    fprintf(gnuplot, "set title '%s'\n", options.title.c_str());
    fprintf(gnuplot, "set xlabel '%s'\n", options.x_title.c_str());
    fprintf(gnuplot, "set ylabel '%s'\n", options.y_title.c_str());
    fprintf(gnuplot, "set format x '%s'\n", options.x_format.c_str());
    fprintf(gnuplot, "set format y '%s'\n", options.y_format.c_str());
    if (!options.y2_title.empty()) {
        fprintf(gnuplot, "set y2label '%s'\n", options.y2_title.c_str());
        fprintf(gnuplot, "set format y2 '%s'\n", options.y2_format.c_str());
        fprintf(gnuplot, "set ytics nomirror\n");
        fprintf(gnuplot, "set y2tics\n");
    }
    fprintf(gnuplot, "set grid\n");
    fprintf(gnuplot, "set term qt font \"Arial\"\n");

    fprintf(gnuplot, "plot ");
    for (size_t i = 0; i < series.size(); ++i) {
        const auto &s = series[i];
        const char *axes = s.use_y2 ? "axes x1y2 " : "";
        const char *delim = i + 1 < series.size() ? ", \\\n     " : "\n";
        fprintf(gnuplot, "'-' using 1:2 %swith lines lw 2 title '%s'%s", axes, s.title.c_str(), delim);
    }
    for (const auto &s : series) {
        for (const auto &[x, y] : s.points) {
            fprintf(gnuplot, "%.2f %.2f\n", x, y);
        }
        fprintf(gnuplot, "e\n");
    }
    pclose(gnuplot);
}