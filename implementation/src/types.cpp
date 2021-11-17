/**
 * Implementation file to hold global types memory
 *
 * Author: Kasimir Gabert
 *
 * Copyright 2021 National Technology & Engineering Solutions of Sandia, LLC
 * (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
 * Government retains certain rights in this software.
 *
 * Please see the LICENSE.md file for license information.
 */

#include "types.hpp"

volatile sig_atomic_t global_shutdown = 0;

localnum_t local_base = 1;
localnum_t local_max = 200;
