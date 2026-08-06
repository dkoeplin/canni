#include "Event.h"

#include <iostream>

Once::Once(const std::string &name, const Day &date, const std::function<void(Day)> &func)
    : Event(name), date_(date), func_(func) {}

void Once::evaluate(const Day& day) {
    return_if(occurred_ || day < date_);
    occurred_ = true;
    func_(day);
}

Monthly::Monthly(const std::string &name, Day start, const std::function<void(Day)> &func)
    : Event(name), start_(start), func_(func) {}

void Monthly::evaluate(const Day& day) {
    return_if(day < start_ || (end_ && day > *end_)); // Skip if not in range.

    while (!prev_ || day >= prev_->next_month()) {
        prev_ = prev_ ? prev_->next_month() : day;
        func_(*prev_);
    }
}

Yearly::Yearly(const std::string& name, Day start, const std::function<void(Day)>& func)
    : Event(name), start_(start), func_(func) {}

void Yearly::evaluate(const Day& day) {
    return_if(day < start_ || (end_ && day > *end_)); // Skip if not in range.

    while (!prev_ || day >= prev_->next_year()) {
        prev_ = prev_ ? prev_->next_year() : day;
        func_(*prev_);
    }
}
