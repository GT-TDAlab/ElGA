/**
 * Internal test script to ensure the vagents are compressed
 *
 * Author: Kasimir Gabert
 *
 * Copyright 2021 National Technology & Engineering Solutions of Sandia, LLC
 * (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
 * Government retains certain rights in this software.
 *
 * Please see the LICENSE.md file for license information.
 */

#include "participant.hpp"

namespace elga {

    class VAgentTest : public Participant {
        public:
            VAgentTest(const ZMQAddress &directory_master) :
                Participant(ZMQAddress(), directory_master, false) { }

            int confirm_agents() {
                while (!ready_ && do_poll()) { }

                // Ensure the right number of agents and virtual agents
                if (num_agents_ != 5) return 1;
                if (num_vagents_ != 500) return 2;
                return 0;
            }
    };

}

using namespace elga;

int main(int argc, char **argv) {
    if (argc != 1) return EXIT_FAILURE;

    elga::ZMQChatterbox::Setup(1);

    ZMQAddress directory_master {"127.0.2.1", 0};
    VAgentTest va {directory_master};

    return va.confirm_agents();
}
