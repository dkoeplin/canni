#pragma once

#include <random>
#include <vector>

#include "canni/Macros.h"
#include "canni/account/Account.h"

namespace canni {

class MarketReturns {
public:
    struct AssetClass {
        Account::Kind type; // Asset type
        double mean;        // Expected annual return (e.g. 0.07)
        double sigma;       // Annual standard deviation (e.g. 0.15)
    };

    /// Preallocates [years] annual returns for each asset class via log-normal sampling.
    MarketReturns(I64 base_year, I64 years, const std::vector<AssetClass> &classes, std::mt19937 &rng);

    pure double at(Account::Kind type, I64 year) const;
    pure double cumulative(Account::Kind type, I64 year) const;

private:
    I64 base_year_ = 0;
    std::vector<std::vector<double>> classes_;
    std::vector<std::vector<double>> cumulative_;
};

} // namespace canni
