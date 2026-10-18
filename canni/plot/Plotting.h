#pragma once

#include <algorithm>
#include <fstream>
#include <iostream>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "Series.h"
#include "canni/data/Day.h"
#include "canni/data/USD.h"

namespace canni {

/// Returns the path to out/<title>.<extension> in the working directory, creating out/ if needed.
/// Characters in [title] other than letters, digits, '_' and '-' are replaced with '_'.
std::string output_path(const std::string &title, const std::string &extension);

struct XYPlotOptions {
    std::string title    = "Plot";
    std::string x_title  = "Time";
    std::string y_title  = "Balance";
    std::string y2_title = "Y2";
    std::string x_format  = "TIME";       // gnuplot format string for x-axis
    std::string y_format  = "$%.2s%c";    // gnuplot format string for left y-axis
    std::string y2_format = "$%.2s%c";    // gnuplot format string for right y-axis

    std::string filename() const;
};

namespace detail {

enum Axis { kX, kY, kY2 };

FILE *setup_plot(const XYPlotOptions &options, bool has_y2, bool stacked);
void setup_numeric_axis(FILE *plot, Axis axis);

template <typename T>
struct AxisFormat {
    static void setup_axis(FILE *plot, const Axis axis) { setup_numeric_axis(plot, axis); }
    static std::string str(const T &data) { return std::to_string(data); }
};

template <>
struct AxisFormat<Day> {
    static void setup_axis(FILE *plot, Axis axis);
    static std::string str(const Day &data) { return data.to_string("%Y-%m-%d"); }
};
template <>
struct AxisFormat<USD> {
    static void setup_axis(FILE *plot, Axis axis);
    static std::string str(const USD &data) { return std::to_string(data.f64()); }
};

} // namespace detail

/// Shows an X/Y line plot with optional dual y-axes.
template <typename X, typename Y, bool Stacked = false>
void xy_plot(const std::vector<Series<X, Y>> &series, const XYPlotOptions &options = {}) {
    const bool has_y2 = std::ranges::any_of(series, [](const auto &data){ return data.use_y2; });
    auto *plot = detail::setup_plot(options, has_y2, Stacked);
    return_if (!plot);
    detail::AxisFormat<X>::setup_axis(plot, detail::Axis::kX);
    detail::AxisFormat<Y>::setup_axis(plot, detail::Axis::kY);
    if (has_y2) {
        detail::AxisFormat<Y>::setup_axis(plot, detail::Axis::kY2);
    }
    fprintf(plot, "plot ");
    const auto last = std::prev(series.rend());
    for (auto s = series.rbegin(); s != series.rend(); ++s) {
        const char *axes = s->use_y2 ? "axes x1y2 " : "";
        const char *delim = s != last ? ", \\\n     " : "\n";
        if constexpr (Stacked) {
            fprintf(plot, "'-' using 1:2 %swith filledcurves x1 title '%s'%s", axes, s->name.c_str(), delim);
        } else {
            fprintf(plot, "'-' using 1:2 %swith lines lw 2 title '%s'%s", axes, s->name.c_str(), delim);
        }
    }
    for (I64 i = static_cast<I64>(series.size()) - 1; i >= 0; --i) {
        const auto &s = series[i];
        for (U64 j = 0; j < s.points.size(); ++j) {
            const X &x = s.points[j].first;
            Y y = s.points[j].second;
            if constexpr (Stacked) {
                for (I64 ii = 0; ii < i; ++ii)
                    y += series[ii].points[j].second;
            }
            fprintf(plot, "%s %s\n", detail::AxisFormat<X>::str(x).c_str(), detail::AxisFormat<Y>::str(y).c_str());
        }
        fprintf(plot, "e\n");
    }
    pclose(plot);
    std::cout << "Wrote " << options.filename() << std::endl;
}

/// Writes [series] to out/<title>.csv with one row per X value and one column per series.
/// Cells are left empty where a series has no point at that X value.
template <typename X, typename Y>
void write_csv(const std::vector<Series<X, Y>> &series, const XYPlotOptions &options = {}) {
    const auto quote = [](const std::string &str) {
        std::string quoted = "\"";
        for (const char c : str) {
            quoted += c == '"' ? "\"\"" : std::string(1, c);
        }
        return quoted + "\"";
    };
    std::map<X, std::vector<std::optional<Y>>> rows;
    for (U64 i = 0; i < series.size(); ++i) {
        for (const auto &[x, y] : series[i].points) {
            auto &row = rows[x];
            row.resize(series.size());
            row[i] = y;
        }
    }
    const std::string path = output_path(options.title, "csv");
    std::ofstream out(path);
    out << quote(options.x_title);
    for (const auto &s : series) {
        out << "," << quote(s.name);
    }
    out << "\n";
    for (const auto &[x, row] : rows) {
        out << detail::AxisFormat<X>::str(x);
        for (const auto &y : row) {
            out << ",";
            if (y) out << detail::AxisFormat<Y>::str(*y);
        }
        out << "\n";
    }
    std::cout << "Wrote " << path << std::endl;
}

template <typename X, typename Y>
void stacked_plot(const std::vector<Series<X, Y>> &series, const XYPlotOptions &options = {}) {
    xy_plot<X, Y, true>(series, options);
}

} // namespace canni
