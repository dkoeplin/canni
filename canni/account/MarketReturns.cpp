#include "MarketReturns.h"

namespace canni {

namespace {

void sample_returns(std::vector<double> &asset, std::vector<double> &cumulative,
                    I64 years, double mean, double sigma, std::mt19937 &rng) {
    // Log-normal: draw log(1+r) ~ Normal(log_mu, sigma) so that E[1+r] = 1+mean.
    const double log_mu = std::log(1.0 + mean) - 0.5 * sigma * sigma;
    std::normal_distribution<double> dist(log_mu, sigma);
    asset = std::vector<double>(years);
    cumulative = std::vector<double>(years);
    for (I64 i = 0; i < years; ++i) {
        asset[i] = std::exp(dist(rng)) - 1.0;
        cumulative[i] = i == 0 ? 1.0 : cumulative[i - 1] * (1.0 + asset[i - 1]);
    }
}

} // namespace

MarketReturns::MarketReturns(const I64 base_year, const I64 years, const std::vector<AssetClass> &classes, std::mt19937 &rng)
    : base_year_(base_year) {
    classes_.resize(Account::Kind::kNUM_ACCOUNT_TYPES);
    cumulative_.resize(Account::Kind::kNUM_ACCOUNT_TYPES);
    for (const auto &[type, mean, sigma] : classes) {
        auto &asset = classes_[static_cast<U64>(type)];
        auto &cumulative = cumulative_[static_cast<U64>(type)];
        sample_returns(asset, cumulative, years, mean, sigma, rng);
    }
}

pure double MarketReturns::at(const Account::Kind type, I64 year) const {
    const auto &sequence = classes_.at(type);
    const I64 idx = year - base_year_;
    return (idx >= 0 && idx < static_cast<I64>(sequence.size())) ? sequence[idx] : 0.0;
}

pure double MarketReturns::cumulative(const Account::Kind type, I64 year) const {
    const auto &sequence = cumulative_.at(type);
    const I64 idx = year - base_year_;
    return (idx >= 0 && idx < static_cast<I64>(sequence.size())) ? sequence[idx] : 0.0;
}

} // namespace canni