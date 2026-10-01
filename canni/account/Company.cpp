#include "Company.h"

#include <map>
#include <utility>

#include "canni/account/Options.h"

namespace canni {

struct Company::Impl {
    explicit Impl(std::string name) : name_(std::move(name)) {}
    std::string name_;
    std::vector<Options *> options_;
    std::map<Day, USD> fmv_; // date -> known FMV per share
    std::map<Day, USD> val_;
};

Company::Company(const std::string &name)
    : impl_(std::make_shared<Impl>(name)) {}

pure std::string Company::name() const { return impl_->name_; }

Company &Company::valuation(const Day &day, const USD &fmv, const USD &ext) {
    impl_->fmv_[day] = fmv;
    impl_->val_[day] = ext;
    return *this;
}

USD Company::fmv_at(const Day &day) const {
    return_if (impl_->fmv_.empty(), 0.0_USD);
    const auto it = impl_->fmv_.upper_bound(day); // Returns the position of the first entry AFTER [day]
    return_if(it == impl_->fmv_.begin(), 0.0_USD);
    return std::prev(it)->second;
}

pure USD Company::market(const Day &day) const {
    return_if (impl_->val_.empty(), 0.0_USD);
    const auto it = impl_->val_.upper_bound(day); // Returns the position of the first entry AFTER [day]
    return_if(it == impl_->val_.begin(), 0.0_USD);
    return std::prev(it)->second;
}

pure const std::vector<Options *> &Company::options() const { return impl_->options_; }

void Company::add_options(Options *grant) const { impl_->options_.push_back(grant); }

void Company::leave_on(Day day) const {
    const Day expiration = day + 3_months;
    for (Options *grant : options()) {
        grant->set_exp(expiration);
    }
}

} // namespace canni
