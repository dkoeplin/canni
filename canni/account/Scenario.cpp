#include "Scenario.h"

namespace canni {

Scenario Scenario::operator|(const Scenario &other) const {
    const std::string combined = name.empty()        ? other.name
                               : other.name.empty()  ? name
                               : name + " + " + other.name;
    auto a = apply, b = other.apply;
    return {combined, [a, b](Portfolio &s) {
        if (a) a(s);
        if (b) b(s);
    }};
}

} // namespace canni
