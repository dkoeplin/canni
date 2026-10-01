#pragma once

#include <memory>
#include <string>
#include <vector>

#include "canni/Macros.h"
#include "canni/data/Day.h"
#include "canni/data/USD.h"

namespace canni {

struct Options;

class Company {
  public:
    explicit Company(const std::string &name);

    pure std::string name() const;

    /// Records a known FMV and rough external price starting on the given day.
    Company &valuation(const Day &day, const USD &fmv, const USD &ext);

    /// Returns the current FMV per share on or before [day], or USD(0) if none is set.
    pure USD fmv_at(const Day &day) const;

    /// Returns the estimated market price per share on or before [day], or USD(0) if none is set.
    pure USD market(const Day &day) const;

    pure const std::vector<Options *> &options() const;

    void add_options(Options *) const;

    void leave_on(Day) const;

  protected:
    struct Impl;
    std::shared_ptr<Impl> impl_;
};

} // namespace canni