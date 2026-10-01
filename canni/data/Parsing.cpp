#include "Parsing.h"

#include <algorithm>
#include <fstream>

#include "canni/Macros.h"

namespace canni {

namespace {
/// Returns the iterator of the next delimiter, skipping those escaped by double quotes.
std::string::const_iterator next_unescaped_delimiter(const std::string::const_iterator &iter,
                                                     const std::string::const_iterator &end,
                                                     const char delim = ',') {
    bool in_quotes = false;
    for (auto j = iter; j != end; ++j) {
        if (*j == '"') in_quotes = !in_quotes;
        else if (*j == delim && !in_quotes) return j;
    }
    return end;
}

void get_row(std::vector<std::string> &row, const std::string &line, const char delim) {
    std::string::const_iterator start = line.begin();
    const auto end = line.end();
    while (start != end) {
        const auto next = next_unescaped_delimiter(start, end, delim);
        std::string str(start, next);
        start = next == end ? end : next + 1;

        str.erase(std::remove_if(str.begin(), str.end(), [](char c) {
            return c == '\"' || c == '$' || c == ',' || c == '\r';
        }), str.end());
        row.push_back(std::move(str));
    }
}

} // namespace

std::vector<std::vector<std::string>> parse_data(const std::string &filename, const char delim) {
    std::ifstream file(filename);
    ASSERT(file.is_open(), "Failed to open file: " << filename);

    std::vector<std::vector<std::string>> matrix;
    std::string line;
    while (std::getline(file, line)) {
        matrix.emplace_back();
        get_row(matrix.back(), line, delim);
    }
    return matrix;
}

} // namespace canni
