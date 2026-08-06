#include "Parsing.h"

#include <algorithm>
#include <fstream>

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

I64 count_unescaped_delim(const std::string &str, const char delim = ',') {
    I64 count = 0;
    for (auto iter = str.begin(); iter != str.end(); iter = next_unescaped_delimiter(iter, str.end(), delim)) {
        ++count;
        ++iter;
    }
    return count - 1;
}

void get_row(nvl::Tensor<2, std::string> &data, I64 row, const std::string &line, const char delim) {
    nvl::Pos<2> idx (row, 0);
    std::string::const_iterator start = line.begin();
    const auto end = line.end();
    while (start != end) {
        const auto next = next_unescaped_delimiter(start, end, delim);
        std::string &str = data[idx] = std::string(start, next);
        start = next == end ? end : next + 1;

        const auto str_end = std::ranges::remove_if(str, [](char c) {
            return c == '\"' || c == '$' || c == ',' || c == '\r';
        }).begin();
        str.erase(str_end, str.end());
        idx[1] += 1;
    }
}

} // namespace

nvl::Tensor<2, std::string> parse_data(const std::string &filename, const char delim) {
    std::ifstream file(filename);
    ASSERT(file.is_open(), "Failed to open file: " << filename);

    std::string line;
    I64 cols = 0;
    I64 rows = 0;
    while (std::getline(file, line)) {
        cols = std::max<I64>(cols, count_unescaped_delim(line, delim) + 1);
        rows += 1;
    }
    auto matrix = nvl::Tensor<2, std::string>(nvl::Pos<2>(rows, cols), "");

    // Reset file position and reread now that the matrix is allocated
    file.clear();
    file.seekg(0);
    I64 row = 0;
    while (std::getline(file, line)) {
        get_row(matrix, row, line, delim);
        row += 1;
    }
    return matrix;
}
