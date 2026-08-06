#pragma once

#include <functional>

#include "nvl/reflect/CastableShared.h"

#include "invest/Day.h"

struct Event;

struct Event {
    class_tag(Event);
    explicit Event(const std::string &name) : name_(name) {}
    virtual ~Event() = default;
    virtual void evaluate(const Day &day) = 0;
    std::string name_;
};

/**
 * @struct Once
 * @brief An event that occurs exactly once.
 * Executes [func] exactly once on the first evaluated date that is on or after the supplied date.
 */
struct Once : Event {
    class_tag(Once, Event);
    explicit Once(const std::string &name, const Day &date, const std::function<void(Day)> &func);
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
    class_tag(Monthly, Event);

    explicit Monthly(const std::string &name, Day start, const std::function<void(Day)> &func);
    void evaluate(const Day &day) override;

    Monthly &ending(const Day &day) { end_ = day; return *this; }

    const Day start_;
    nvl::Maybe<Day> end_ = nvl::None;
    nvl::Maybe<Day> prev_;
    const std::function<void(Day)> func_;
};

struct Yearly : Event {
    class_tag(Yearly, Event);

    explicit Yearly(const std::string &name, Day start, const std::function<void(Day)> &func);
    void evaluate(const Day &day) override;

    Yearly &ending(const Day &day) { end_ = day; return *this; }

    const Day start_;
    nvl::Maybe<Day> end_ = nvl::None;
    nvl::Maybe<Day> prev_;
    const std::function<void(Day)> func_;
};