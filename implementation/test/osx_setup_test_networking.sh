#!/bin/sh

# Copyright 2021 National Technology & Engineering Solutions of Sandia, LLC
# (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
# Government retains certain rights in this software.
#
# Please see the LICENSE.md file for license information.

ifconfig lo0 alias 127.0.2.1
ifconfig lo0 alias 127.0.2.3
ifconfig lo0 alias 127.0.2.5
ifconfig lo0 alias 127.0.2.6
ifconfig lo0 alias 127.0.2.7
ifconfig lo0 alias 127.0.3.1
ifconfig lo0 alias 127.0.4.1
ifconfig lo0 alias 127.0.5.1
ifconfig lo0 alias 127.0.6.1
