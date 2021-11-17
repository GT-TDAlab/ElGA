Basic Use
---------

The ElGa binary has the following syntax:

ElGa [-h] [-P num] [-B base] -d directory-ip command [help] [command args...]

If ElGa is responsible for starting multiple threads, the -P option
controls how many to start.  The directory-ip is the single starting point
for ElGa.  If there will be multiple components running on the same IP
address, then the -B command can specify a base offset to begin counting
from.  For example, to start a directory master, directory, and 4 agents
all on the IP address <IP>, the following could be used:

    ./ElGa -d <IP> directory-master &
    ./ElGa -d <IP> -P 1 -B 1 directory <IP> &
    ./ElGa -d <IP> -P 4 -B 2 agent <IP> &

For each command, help is provided by requesting help.  For the list of
commands, -h can be specified to ElGa.  The main commands are:

    agent - launches agents
    directory - starts directory servers
    client - issues client commands
    agent - starts agents
    streamer - starts a streamer

To change the algorithm, edit the src/algorithm.hpp file.

Scripts for Experiments
-----------------------

In this folder there are various scripts that assist with running the
experiments.  They are listed here.

- `cleanup.sh` stops binaries and removes output and log files
- `exp_stream.sh` runs an experiment that streams in a graph and processes batch-by-batch
- `get-dist.sh` returns distribution information for each agent
- `grab.sh` collects the logs for each agent
- `install.sh` useful on a cluster to setup hwloc and numactl dependencies
- `new-agents.sh` starts and controls new agents for elasticity experiments
- `run_exp.sh` used to launch and run an experiments
- `start-elga.sh` starts ElGA across the cluster
- `start-streamers.sh` used to start streamers to listen for streaming edges, e.g., from A-BTER
- `stop-streamers.sh` used to stop the streamer processes
- `stream-file.sh` used to launch streamers across the cluster to read in a graph from a file
- `upload-dumps-raw.sh` used to save a graph that has been streamed into ElGA into a single folder
- `upload-dumps.sh` used to load a graph dumped from ElGA into Hadoop, for example to run with Blogel or GraphX
- `vars.sh` contains parameters for the system
- `watch-agent-logs.sh` helper file to watch the contents of log files

To use the scripts, the following files are assumed:

- `/cscratch/nodes` a file containing a pdsh compatible list of nodes
- `/cscratch/nodes-full` a file containing each node on its own line
- `/cscratch/nodes-all` a file containing each node on its own line
- `/ip` contains the IP address of the current node

ElGa assumes that /scratch/elga exists and is writeable.
