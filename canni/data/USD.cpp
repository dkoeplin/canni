#include "USD.h"

#include <algorithm>

namespace canni {

USD USD::parse(const std::string &str) {
    std::string tmp = str;
    const auto iter = std::ranges::remove_if(tmp, [](const char c) { return c == '$' || c == ','; }).begin();
    tmp.erase(iter, tmp.end());
    if (const auto decimal = std::ranges::find(tmp, '.'); decimal != tmp.end()) {
        const auto neg = !tmp.empty() && tmp[0] == '-';
        const auto str_dollars = std::string(tmp.begin(), decimal);
        auto str_cents = std::string(decimal + 1, tmp.end());
        // Round to the nearest cent if there are more than two decimal digits.
        const bool round_up = str_cents.size() > 2 && str_cents[2] >= '5';
        str_cents.resize(2, '0');
        const bool has_dollars = !str_dollars.empty() && str_dollars != "-";
        const I64 dollars = has_dollars ? std::stoll(str_dollars) : 0;
        const I64 cents = std::stoll(str_cents) + (round_up ? 1 : 0);
        const I64 total = (std::abs(dollars)*100 + cents) * (neg ? -1 : 1);
        return USD(total);
    }
    const I64 dollars = tmp.empty() ? 0 : std::stoll(tmp);
    return USD(dollars * 100);
}

std::string USD::to_string() const {
    const I64 abs_cents = std::abs(cents_);
    const std::string sign = cents_ < 0 ? "-" : "";
    const I64 cents = abs_cents % 100;
    // TODO: Use string interpolation to avoid unnecessary extra copies
    return sign + "$" + std::to_string(abs_cents / 100) + "." + (cents < 10 ? "0" : "") + std::to_string(cents);
}
} // namespace canni
