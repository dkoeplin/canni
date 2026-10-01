#pragma once

#include <functional>
#include <string>
#include <utility>

#include "canni/Macros.h"

namespace canni {

struct Portfolio;

/**
 * @struct Scenario
 * @brief A named, composable event in a Portfolio.
 * Compose with | to create a combined modifier applied in left-to-right order.
 */
struct Scenario {
    Scenario() = default;
    Scenario(std::string name, std::function<void(Portfolio &)> func) : name(std::move(name)), apply(std::move(func)) {}
    pure Scenario operator|(const Scenario &other) const;

    std::string name;
    std::function<void(Portfolio &)> apply;
};

/**
 *
 * @tparam P
 */
template <typename P>
    requires std::is_base_of_v<Portfolio, P>
struct TargetedScenario : Scenario {
    TargetedScenario(std::string name, std::function<void(P &)> func)
        : Scenario(name, [func](Portfolio &p) { func(*static_cast<P *>(&p)); }) {}
};

} // namespace canni
