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

#ifndef TIMER_HPP
#define TIMER_HPP

#include <iostream>
#include <chrono>
#include <string>

namespace timer {

    typedef std::chrono::high_resolution_clock::time_point timer_t;
    typedef std::chrono::duration<double> duration_t;

    class Timer {
        private:
            timer_t start_;
            timer_t end_;
            duration_t time_;
            std::string name_;
        public:
            Timer();
            Timer(std::string name);
            ~Timer();
            void tick();
            void tock();
            void retock();

            duration_t get_time() const;
            void set_time(duration_t new_time);

            void reset() { set_time(duration_t::zero()); }

            friend std::ostream& operator<<(std::ostream& os, const Timer& t);
    };
    std::ostream& operator<<(std::ostream& os, const Timer& t);

    class TimePoint {
        private:
            timer_t point_;
        public:
            TimePoint();

            int64_t distance_us() const;
    };

}

#endif
