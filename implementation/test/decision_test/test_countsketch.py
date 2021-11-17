# Copyright 2021 National Technology & Engineering Solutions of Sandia, LLC
# (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
# Government retains certain rights in this software.
#
# Please see the LICENSE.md file for license information.

import numpy as np
import sys

if len(sys.argv) > 1:
    dist_param = int(sys.argv[1])
else:
    exit()


arg_ind = 1

# Create a zipf dist with given parameters
fname = sys.argv[3]
s = np.random.zipf(dist_param, int(sys.argv[2]))
# s = np.random.randint(low=0, high=1000,  size=int(sys.argv[arg_ind]))

# Write the map to a file
np.savetxt(fname, s, delimiter='\n', fmt="%d")
