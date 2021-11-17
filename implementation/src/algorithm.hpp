/**
 * ElGA Algorithm Class
 *
 * Copyright 2021 National Technology & Engineering Solutions of Sandia, LLC
 * (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
 * Government retains certain rights in this software.
 *
 * Please see the LICENSE.md file for license information.
 */

#ifndef ALGORITHM_HPP_
#define ALGORITHM_HPP_

#ifdef CONFIG_PAGERANK
#include "pralgorithm.hpp"
#endif
#ifdef CONFIG_WCC
#include "wccalgorithm.hpp"
#endif
#ifdef CONFIG_KCORE
#include "kcorealgorithm.hpp"
#endif
#ifdef CONFIG_BFS
#include "bfsalgorithm.hpp"
#endif
#ifdef CONFIG_LPA
#include "lpaalgorithm.hpp"
#endif

#endif
