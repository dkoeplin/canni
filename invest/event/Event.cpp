#include "Event.h"

#include <iostream>
#include <utility>

Once::Once(Accounts *parent, std::string name, Day date, const std::function<void(Day)> &func)
    : Event(parent, std::move(name)), date_(date), func_(func) {}

void Once::evaluate(const Day& day) {
    return_if(occurred_ || day < date_);
    occurred_ = true;
    func_(day);
}

Monthly::Monthly(Accounts *parent, std::string name, Day start, const std::function<void(Day)> &func)
    : Event(parent, std::move(name)), start_(start), func_(func) {}

void Monthly::evaluate(const Day& day) {
    return_if(day < start_ || (end_ && day > *end_)); // Skip if not in range.

    while (!prev_ || day >= prev_->next_month()) {
        prev_ = prev_ ? prev_->next_month() : day;
        func_(*prev_);
    }
}

Yearly::Yearly(Accounts *parent, std::string name, Day start, const std::function<void(Day)>& func)
    : Event(parent, std::move(name)), start_(start), func_(func) {}

void Yearly::evaluate(const Day& day) {
    return_if(day < start_ || (end_ && day > *end_)); // Skip if not in range.

    while (!prev_ || day >= prev_->next_year()) {
        prev_ = prev_ ? prev_->next_year() : day;
        func_(*prev_);
    }
}
