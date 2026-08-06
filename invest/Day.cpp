#include "Day.h"

#include <algorithm>
#include <array>
#include <iomanip>
#include <sstream>

#include "nvl/data/Maybe.h"
#include "nvl/macros/ReturnIf.h"

nvl::Maybe<Day> Day::parse(std::string_view str, const nvl::Maybe<std::string>& format) {
    if (!format.has_value()) {
        const auto s0 = str.find('/');
        return_if(s0 == std::string::npos, nvl::None);
        const auto s1 = str.find('/', s0 + 1);
        return_if(s1 == std::string::npos, nvl::None);
        const I64 month = std::stoi(std::string(str.substr(0, s0))) - 1;
        const I64 day = std::stoi(std::string(str.substr(s0 + 1, s1 - s0)));
        const I64 year = std::stoi(std::string(str.substr(s1 + 1, str.size() - s1)));
        return Day(day, static_cast<Month>(month), year);
    }

    std::tm date = {};
    std::string s (str);
    std::istringstream ss (s);
    ss >> std::get_time(&date, format->c_str());
    return_if(ss.fail(), nvl::None);
    return Day(date.tm_mday, static_cast<Month>(date.tm_mon), date.tm_year + 1900);
}

Day::Day(I64 day, Month month, I64 year) : year_(year), month_(static_cast<I64>(month)), day_(day) {}

std::string Day::to_string(const nvl::Maybe<std::string>& format) const {
    std::stringstream ss;
    if (!format.has_value()) {
        ss << (month_ + 1 < 10 ? "0" : "") << (month_ + 1) << "/" << (day_ < 10 ? "0" : "") << day_ << "/" << year_;
    } else {
        std::tm date = {};
        date.tm_mday = static_cast<int>(day_);
        date.tm_mon = static_cast<int>(month_);
        date.tm_year = static_cast<int>(year_ - 1900);
        ss << std::put_time(&date, format->c_str());
    }
    return ss.str();
}

pure I64 Day::day() const { return day_; }
pure Month Day::month() const { return static_cast<Month>(month_); }
pure I64 Day::year() const { return year_; }

pure Day Day::next_month() const {
    Day next = *this;
    const auto prev_month = month_;
    next.day_ = 1;
    next.month_ = (static_cast<I64>(prev_month) + 1) % 12;
    next.year_ += static_cast<I64>(prev_month) == 11 ? 1 : 0;
    return next;
}

pure Day Day::next_year() const {
    Day next = *this;
    next.year_ += 1;
    return next;
}

static I64 to_jdn(const I64 y, const I64 m, const I64 d) {
    const I64 a = (14 - m) / 12;
    const I64 Y = y + 4800 - a;
    const I64 M = m + 12 * a - 3;
    return d + (153 * M + 2) / 5 + 365 * Y + Y / 4 - Y / 100 + Y / 400 - 32045;
}

pure I64 Day::operator-(const Day &rhs) const {
    return to_jdn(year_, month_ + 1, day_) - to_jdn(rhs.year_, rhs.month_ + 1, rhs.day_);
}

pure Day Day::operator+(const Distance &duration) const {
    Day next = *this;
    next.month_ = (month_ + duration.months) % 12;
    next.year_ += duration.years + (month_ + duration.months) / 12;
    return next;
}
