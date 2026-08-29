// Copyright Contributors to the DNF5 project.
// Copyright Contributors to the libdnf project.
// SPDX-License-Identifier: LGPL-2.1-or-later
//
// This file is part of libdnf: https://github.com/rpm-software-management/libdnf/
//
// Libdnf is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 2.1 of the License, or
// (at your option) any later version.
//
// Libdnf is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with libdnf.  If not, see <https://www.gnu.org/licenses/>.

#include "libdnf5/conf/option_seconds.hpp"

#include "libdnf5/utils/bgettext/bgettext-mark-domain.h"

#include <cctype>
#include <limits>

namespace {

constexpr double SECONDS_IN_MINUTE = 60;
constexpr double SECONDS_IN_HOUR = 60 * SECONDS_IN_MINUTE;
constexpr double SECONDS_IN_DAY = 24 * SECONDS_IN_HOUR;
constexpr double SECONDS_IN_WEEK = 7 * SECONDS_IN_DAY;

double iso8601_duration_to_seconds(const std::string & value) {
    double seconds = 0;
    bool in_time_part = false;
    bool any_component = false;

    for (std::size_t pos = 1; pos < value.length();) {
        if (value[pos] == 'T' || value[pos] == 't') {
            if (in_time_part) {
                throw libdnf5::OptionInvalidValueError(
                    M_("Invalid ISO 8601 duration \"{}\", duplicate time part separator \"T\""), value);
            }
            in_time_part = true;
            ++pos;
            continue;
        }

        double number;
        std::size_t idx;
        try {
            number = std::stod(value.substr(pos), &idx);
        } catch (...) {
            throw libdnf5::OptionInvalidValueError(M_("Invalid ISO 8601 duration \"{}\""), value);
        }
        if (number < 0) {
            throw libdnf5::OptionInvalidValueError(
                M_("Invalid ISO 8601 duration \"{}\", negative values not allowed"), value);
        }
        pos += idx;
        if (pos >= value.length()) {
            throw libdnf5::OptionInvalidValueError(
                M_("Invalid ISO 8601 duration \"{}\", missing unit designator"), value);
        }

        const char designator = static_cast<char>(std::toupper(static_cast<unsigned char>(value[pos++])));
        switch (designator) {
            case 'W':
            case 'D':
                if (in_time_part) {
                    throw libdnf5::OptionInvalidValueError(
                        M_("Invalid ISO 8601 duration \"{}\", date unit '{}' used in the time part"),
                        value,
                        std::string(1, designator));
                }
                seconds += number * (designator == 'W' ? SECONDS_IN_WEEK : SECONDS_IN_DAY);
                break;
            case 'H':
            case 'S':
                if (!in_time_part) {
                    throw libdnf5::OptionInvalidValueError(
                        M_("Invalid ISO 8601 duration \"{}\", time unit '{}' used before the \"T\" separator"),
                        value,
                        std::string(1, designator));
                }
                seconds += number * (designator == 'H' ? SECONDS_IN_HOUR : 1);
                break;
            case 'M':
                if (!in_time_part) {
                    throw libdnf5::OptionInvalidValueError(
                        M_("Invalid ISO 8601 duration \"{}\", months and years are not supported, "
                           "use weeks or days instead"),
                        value);
                }
                seconds += number * SECONDS_IN_MINUTE;
                break;
            case 'Y':
                throw libdnf5::OptionInvalidValueError(
                    M_("Invalid ISO 8601 duration \"{}\", months and years are not supported, "
                       "use weeks or days instead"),
                    value);
            default:
                throw libdnf5::OptionInvalidValueError(
                    M_("Invalid ISO 8601 duration \"{}\", unknown unit designator '{}'"),
                    value,
                    std::string(1, value[pos - 1]));
        }
        any_component = true;
    }

    if (!any_component) {
        throw libdnf5::OptionInvalidValueError(M_("Invalid ISO 8601 duration \"{}\", no time component"), value);
    }

    return seconds;
}

libdnf5::OptionSeconds::ValueType to_value_type(double seconds, const std::string & value) {
    if (seconds > static_cast<double>(std::numeric_limits<libdnf5::OptionSeconds::ValueType>::max())) {
        throw libdnf5::OptionInvalidValueError(M_("Time option value \"{}\" is too large"), value);
    }
    return static_cast<libdnf5::OptionSeconds::ValueType>(seconds);
}

}  // namespace

namespace libdnf5 {

OptionSeconds::OptionSeconds(ValueType default_value, ValueType min, ValueType max)
    : OptionNumber(default_value, min, max) {}

OptionSeconds::OptionSeconds(ValueType default_value, ValueType min) : OptionNumber(default_value, min) {}

OptionSeconds::OptionSeconds(ValueType default_value) : OptionNumber(default_value, -1) {}

OptionSeconds::ValueType OptionSeconds::from_string(const std::string & value) const {
    if (value.empty()) {
        throw OptionInvalidValueError(M_("Empty time option value"));
    }

    if (value == "-1" || value == "never") {  // Special cache timeout, meaning never
        return -1;
    }

    if (value.front() == 'P' || value.front() == 'p') {
        return to_value_type(iso8601_duration_to_seconds(value), value);
    }

    std::size_t idx;
    double res;
    try {
        res = std::stod(value, &idx);
    } catch (...) {
        throw OptionInvalidValueError(M_("Invalid time option value \"{}\", number or \"never\" expected"), value);
    }
    if (res < 0) {
        throw OptionInvalidValueError(
            M_("Invalid time option value \"{}\", negative values except \"-1\" not allowed"), value);
    }

    if (idx < value.length()) {
        if (idx < value.length() - 1) {
            throw OptionInvalidValueError(M_("Unknown time format \"{}\""), value);
        }
        switch (value.back()) {
            case 's':
            case 'S':
                break;
            case 'm':
            case 'M':
                res *= SECONDS_IN_MINUTE;
                break;
            case 'h':
            case 'H':
                res *= SECONDS_IN_HOUR;
                break;
            case 'd':
            case 'D':
                res *= SECONDS_IN_DAY;
                break;
            case 'w':
            case 'W':
                res *= SECONDS_IN_WEEK;
                break;
            default:
                throw OptionInvalidValueError(M_("Unknown time unit '{}'"), std::string(&value.back(), 1));
        }
    }

    return to_value_type(res, value);
}

void OptionSeconds::set(Priority priority, const std::string & value) {
    set(priority, from_string(value));
}

void OptionSeconds::set(const std::string & value) {
    set(Priority::RUNTIME, value);
}

}  // namespace libdnf5
