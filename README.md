Overview
========

ElGA (Elastic Graph Analysis) is a graph analysis system that is elastic---it
can scale during operation---while supporting dynamic, continuously
changing graphs. It has built-in support for two applications, PageRank and Weakly Connected Components.

💻 **Source Code:** [http://github.com/GT-TDAlab/ElGA]  
📘 **Documentation:** [http://gt-tdalab.github.io/ElGA/]

ElGA is developed by the members of [GT-TDAlab](http://tda.gatech.edu) and
[Sandia National Laboratories](https://www.sandia.gov).

License
-------

ElGA is distributed under BSD License. For details, see [`LICENSE.md`](LICENSE.md).

Contributors
------------

- [Kasimir Gabert](https://kasimir.co)
- [Kaan Sancak](https://www.kaansancak.com/)
- M. Yusuf Özkaya
- [Ali Pınar](https://old-www.sandia.gov/~apinar/)
- [Ümit V. Çatalyürek](https://www.cc.gatech.edu/~umit/)

Contact
-------

For questions or support [open an issue](../../issues) or contact contributors
via <tdalab@cc.gatech.edu>.

Citation
--------

The citation information for this paper is as follows (BibTeX):

```bibtex
@inproceedings { Gabert21-SC,
    title = {{ElGA}: Elastic and Scalable Dynamic Graph Analysis},
    author = {Kasimir Gabert and Kaan Sancak and M. Yusuf \"{O}zkaya and Ali P{\i}nar and \"Umit V. \c{C}ataly\"{u}rek},
    booktitle = {SC21: International Conference for High Performance Computing, Networking, Storage and Analysis},
    month = {Nov},
    year = {2021},
    doi = {10.1145/3458817.3480857}
}
```

Quick Start
===========

The fastest way to get started is to create a docker image that compiles ElGA
with the necessary dependencies.  To do this, make sure that Docker is
installed, and docker containers have sufficient memory (at least 8GB).  Then,
run:

    docker build -t elga .

from this directory.

Finally, to use ElGA, run:

    docker run --rm -it elga

This will place you into a docker container that has the ElGA binary.
Inside, you can run:

    $ ./start-elga.sh

Note that the Docker container is configured to start a tmux session when you
connect to it.

Now, a (small) cluster inside of the docker contain will be started.  You
can see the progress of agents in `/scratch/elga/elga.agent.*.log`.

To run an example computation, you can perform the following:

    $ ./elga.sh streamer +el ~/graphs/email-EuAll.txt

You can confirm it has been loaded by running

    $ tail -n 1 /scratch/elga/elga.agent.0.0.log

Note that this will not yet show the global number of edges or vertices,
only the specific agents.

You should see something like:

    [ElGa : Agent :  2] HRTBEAT | - rate=0 state=0 batch=0 nV=18,173 nE=111,440 gnV=0 gnE=0 pending=0 ia=0 d=0 amn=0 uan=0

Next, you can run the currently installed application, WCC.

    $ ./elga.sh client start

Progress can be tracked by viewing the agent log:

    $ tail -n 15 /scratch/elga/elga.agent.0.0.log

    [ElGa : Agent :  2] HRTBEAT | - rate=0 state=0 batch=0 nV=18,173 nE=111,440 gnV=0 gnE=0 pending=0 ia=0 d=0 amn=0 uan=0
    [ElGa : Agent :  2] UPDATE  | [update] 0.169201
    [ElGa : Agent :  2] SUP STP | [superstep] 0.0918469
    [ElGa : Agent :  2] SUP STP | [superstep] 0.086071
    [ElGa : Agent :  2] SUP STP | [superstep] 0.0463666
    [ElGa : Agent :  2] SUP STP | [superstep] 0.00721667
    [ElGa : Agent :  2] SUP STP | [superstep] 0.00137144
    [ElGa : Agent :  2] SUP STP | [superstep] 0.00106937
    [ElGa : Agent :  2] SUP STP | [superstep] 0.00116135
    [ElGa : Agent :  2] SUP STP | [superstep] 0.000974795
    [ElGa : Agent :  2] SUP STP | [superstep] 0.000961325
    [ElGa : Agent :  2] SUP STP | [superstep] 0.000952088
    [ElGa : Agent :  2] RESET   |
    [ElGa : Agent :  2] B TIME  | [batch] 0.239276
    [ElGa : Agent :  2] HRTBEAT | - rate=0 state=2 batch=1 nV=64,599 nE=111,440 gnV=265,214 gnE=420,045 pending=0 ia=64,599 d=0 amn=0 uan=0

The algorithm output can then be saved:

    $ ./elga.sh client save

It is now available in `/scratch/elga/*.out`:

    $ cat /scratch/elga/*.out | grep -v ' 0$' | sort -n > /tmp/eu-nonzero

This can be compared against ground-truth:

    $ if [ "$(md5sum /tmp/eu-nonzero | cut -f1 -d' ')" == "f512ddc8e2808966c3a1121a6c61a507" ]; then echo "All good"; fi

Finally, everything can be cleaned up:

    $ ./cleanup.sh

To change the settings, e.g., the algorithm being run, use `cmake` from `/scratch/elga/build`:

    $ pushd /scratch/elga/build && cmake -DALG=PR ~/elga && make -j `grep -c ^processor /proc/cpuinfo` && popd

Building ElGA
-------------

To build ElGA, make sure you have [ZeroMQ](https://zeromq.org/download/)
installed and a modern development environment, including CMake. Make sure all
submodules are checked out and initialized (`git submodule update --init
--recursive`).  Then, inside of the `build/` directory run:

    cmake ..
    make -j `grep -c ^processor /proc/cpuinfo`

### Building Without System-Wide ZeroMQ

Alternatively, if you cannot install ZeroMQ as part of your system, you can
clone and make from their repository, defining the ZeroMQ directory as an
environment variable (You can install ZeroMQ at any location you want. It does
not have to be under ElGA.):

    #install ZeroMQ
    git clone https://github.com/zeromq/libzmq.git --depth 1 --branch v4.3.4
    cd libzmq
    export ZeroMQ_ROOT=$PWD
    mkdir release
    cd release
    cmake -DCMAKE_BUILD_TYPE=Release ..
    make -j `grep -c ^processor /proc/cpuinfo`

Then, provide the path to the ZeroMQ release folder and make ElGA, again from
the `build/` directory:

    ZeroMQ_DIR=ZeroMQ_ROOT/release cmake ..
    make -j `grep -c ^processor /proc/cpuinfo`

ElGA has been tested in the following configurations:

- CentOS 8.4 with ZeroMQ 4.3.4 (latest version of ZeroMQ at this moment).

### Configuring ElGA

In order to configure ElGA, you can use ``cmake``.  To see all configuration
options, you can use a tool such as ``ccmake`` instead of ``cmake``.

For example, to change width of the consistent hash table, you can run:

	cmake -DTABLE_WIDTH=65536 ..
	make -j `grep -c ^processor /proc/cpuinfo`

The algorithm is configured through the setting `ALG`.  There is current
support for the following algorithms:
- *BFS* breadth-first search. Note that when making this setting, starting ElGA
  takes the argument of the vertex to run BFS from, that is, `./elga.sh client
  start <vertex ID>`.
- *WCC* weakly-connected components
- *PR* PageRank
- *LPA* A label propagation algorithm that is synchronous, uses majority
  voting, and breaks ties with the lowest numbered label
- *KCore* A k-core decomposition algorithm that computes coreness values for
  each vertex

To change the algorithm from WCC to PageRank, you can run the following:

	cmake -DALG=PR ..
	make -j `grep -c ^processor /proc/cpuinfo`

Running Experiments and Basic Use
---------------------------------

There is a single executable that ElGA uses to start the various components.
After starting ElGA, it is expected to continuosly run for a single graph. As
data streams in, batches can be processed.

Please see [scripts/README.md](scripts/README.md) for more details on each
provided helper script.

Running ElGA With Singularity
-----------------------------

In some cases it may be difficult to package all of the required libraries
for ElGA.  It is easy to use [Singularity](https://sylabs.io/singularity/)
to launch ElGA on clusters or other systems that support Singularity
images.

First, build the tutorial Docker image:

    docker build -t elga .

Next, convert the Docker image to a singularity image:

    docker image save -o elga.tar elga
    singularity pull elga.sif docker-archive://"$PWD"/elga.tar

The first argument of the sif file are extra compile options to set before
re-compiling. Any remaining arguments will be passed to `ElGA` directly.

For example, suppose you have a cluster with 5 nodes, each with 16 cores,
with IP address `10.0.0.[1-5]`, and Singularity is installed.  We can use
`elga.sif` as follows. On the first node, which will run the directory
master and the directories, we run:

    elga.sif "" -d 10.0.0.1 -B 1 -P 1 directory 10.0.0.1 &
    elga.sif "" -d 10.0.0.1 directory-master

Then, on each of the four compute nodes, we can start agents:

    elga.sif "" -d 10.0.0.1 -P 16 agent 10.0.0.X

Finally, we can stream in a graph, run the computation, and save the
output.
