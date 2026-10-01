#include "USD.h"

#include <algorithm>

namespace canni {

USD USD::parse(const std::string &str) {
    std::string tmp = str;
    const auto iter = std::remove(tmp.begin(), tmp.end(), '$');
    tmp.erase(iter, tmp.end());
    const auto decimal = std::find(tmp.begin(), tmp.end(), '.');
    if (decimal != tmp.end()) {
        const auto str_dollars = std::string(tmp.begin(), decimal);
        const auto str_cents = std::string(decimal + 1, tmp.end());
        const I64 dollars = std::stoll(str_dollars);
        const I64 cents = std::stoll(str_cents);
        const I64 total = (std::abs(dollars)*100 + cents) * (dollars < 0 ? -1 : 1);
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
