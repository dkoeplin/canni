#pragma once

#include <functional>
#include <optional>
#include <utility>

#include "canni/data/Day.h"

namespace canni {

struct Portfolio;

struct Event {
    explicit Event(Portfolio *parent, std::string name) : parent_(parent), name_(std::move(name)) {}
    virtual ~Event() = default;
    virtual void evaluate(const Day &day) = 0;
    Portfolio *parent_;
    std::string name_;
};

/**
 * @struct Once
 * @brief An event that occurs exactly once.
 * Executes [func] exactly once on the first evaluated date that is on or after the supplied date.
 */
struct Once : Event {
    explicit Once(Portfolio *, std::string name, Day date, const std::function<void(Day)> &func);
    void evaluate(const Day &day) override;

    const Day date_;
    bool occurred_ = false;
    const std::function<void(Day)> func_;
};

/**
 * @struct Monthly
 * @brief An event that occurs on the first of each month on or after a start date.
 *
 * Executes [func] N times on each evaluation, where N is the number of expected occurrences
 * that has passed since the previous evaluation.
 *
 * Executes[func] exactly once on the first call.
 */
struct Monthly : Event {
    explicit Monthly(Portfolio *, std::string name, Day start, const std::function<void(Day)> &func);
    void evaluate(const Day &day) override;

    Monthly &ending(const Day &day) { end_ = day; return *this; }

    const Day start_;
    std::optional<Day> end_;
    std::optional<Day> prev_;
    const std::function<void(Day)> func_;
};

/**
 * @struct Yearly
 * @brief An event that occurs on the first day of each year on or after the start date.
 *
 * Executes [func] N times on each evaluation, where N is the number of expected occurrences
 * that has passed since the previous evaluation.
 *
 * Executes[func] exactly once on the first call.
 */
struct Yearly : Event {
    explicit Yearly(Portfolio *, std::string name, Day start, const std::function<void(Day)> &func);
    void evaluate(const Day &day) override;

    Yearly &ending(const Day &day) { end_ = day; return *this; }

    const Day start_;
    std::optional<Day> end_;
    std::optional<Day> prev_;
    const std::function<void(Day)> func_;
};

} // namespace canni