/**
 * ElGA Timer
 * A helper file to easily time various points of execution
 *
 * Author: Kasimir Gabert
 *
 * Copyright 2021 National Technology & Engineering Solutions of Sandia, LLC
 * (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
 * Government retains certain rights in this software.
 *
 * Please see the LICENSE.md file for license information.
 */

#include "timer.hpp"

using namespace timer;

Timer::Timer(std::string name)
        : name_(name), start_(), end_(), time_() { }
Timer::Timer() : name_(""), start_(), end_(), time_() { }
Timer::~Timer() { }
void Timer::tick() { start_ = std::chrono::high_resolution_clock::now(); }
void Timer::tock() {
    end_ = std::chrono::high_resolution_clock::now();
    time_ = end_-start_;
}
void Timer::retock() {
    end_ = std::chrono::high_resolution_clock::now();
    time_ += end_-start_;
}
namespace timer {
    std::ostream& operator<<(std::ostream& os, const Timer& t) {
        os << "[" << t.name_ << "] " << t.time_.count();
        return os;
    }
}

duration_t
Timer::get_time() const {
    return time_;
}

void
Timer::set_time(duration_t new_time) {
    time_ = new_time;
}

TimePoint::TimePoint() : point_(std::chrono::high_resolution_clock::now()) { }

int64_t TimePoint::distance_us() const {
    timer_t now = std::chrono::high_resolution_clock::now();
    return std::chrono::duration_cast<std::chrono::microseconds>(now-point_).count();
}
