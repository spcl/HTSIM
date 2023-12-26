# htsim Network Simulator

htsim is a high performance discrete event simulator, inspired by ns2, but much faster, primarily intended to examine congestion control algorithm behaviour.  It was originally written by [Mark Handley](http://www0.cs.ucl.ac.uk/staff/M.Handley/) to allow [Damon Wishik](https://www.cl.cam.ac.uk/~djw1005/) to examine TCP stability issues when large numbers of flows are multiplexed.  It was extended by [Costin Raiciu](http://nets.cs.pub.ro/~costin/) to examine [Multipath TCP performance](http://nets.cs.pub.ro/~costin/files/mptcp-nsdi.pdf) during the MPTCP standardization process, and models of datacentre networks were added to [examine multipath transport](http://nets.cs.pub.ro/~costin/files/mptcp_dc_sigcomm.pdf) in a variety of datacentre topologies.  [NDP](http://nets.cs.pub.ro/~costin/files/ndp.pdf) was developed using htsim, and simple models of DCTCP, DCQCN were added for comparison.  Later htsim was adopted by Correct Networks (now part of Broadcom) to develop [EQDS](http://nets.cs.pub.ro/~costin/files/eqds.pdf), and switch models were improved to allow a variety of forwarding methods.  Support for a simple RoCE model, PFC, Swift and HPCC were added.

## The basics

There are some limited instructions in the [wiki](https://github.com/Broadcom/csg-htsim/wiki).  

htsim is written in C++, and has no dependencies.  It should compile and run with g++ or clang on MacOS or Linux.  To compile htsim, cd into the sim directory and run make.

To get started with running experiments, take a look in the experiments directory where there are some examples.  These examples generally require bash, python3 and gnuplot.

## Getting started with htsim

Compile with the following instruction. To do so, we recommend running the following command line from the ```/sim``` directory (feel free to change the number of jobs being run in parallel).

```
make clean && cd datacenter/ && make clean && cd .. && make -j 8 && cd datacenter/ && make -j 8 && cd ..
```

It is then possible to run htsim by using three possible methods:
- Using connection matrixes. Details here.
- Using C++ code to setup the simulation directly.
- Using LGS. See the next paragraph for details.


## Basic Instructions for LGS

We currently provide two entry points to run the simulator with the support of LogGOPSim: htsim_uec_entry_modern and htsim_ndp_entry_modern. The idea here is that these entry points will execute the GOAL input file (in binary format) that is given as part of the ```-goal``` command. The input file should be inside ```sim/lgs/input/```.
We include two sample input files (```incast_1024_100_4194304.bin``` and ```incast_1024_8_4194304.bin```).

To actually run the application a typical command line would look like this (from inside the /plotting folder and for SMaRTT protocol):
```
../sim/datacenter/htsim_uec_entry_modern -o uec_entry -k 1 -algorithm delayB -nodes 1024 -q 4452000 -strat ecmp_host_random2_ecn -number_entropies 256 -kmin 20 -kmax 80 -target_rtt_percentage_over_base 50 -use_fast_increase 1 -use_super_fast_increase 1 -fast_drop 1 -linkspeed 800000 -mtu 4096 -seed 919 -queue_type composite  -hop_latency 700 -reuse_entropy 1 -goal incast_1024_8_4194304.bin -x_gain 2 -y_gain 2.5 -w_gain 2 -z_gain 0.8  -collect_data 1 > out.tmp
```

## Generating GOAL input files
We will now explain how to generate a goal inpute file and then how to convert it into a binary format for it to be used as input.

### Manually
It is possible to write a goal input file manually. For the specific of it, please check the original [GOAL](https://htor.inf.ethz.ch/publications/img/hoefler-goal.pdf) paper and [LGS](https://htor.inf.ethz.ch/publications/img/hoefler-loggopsim.pdf) paper.

### Schedgen
Schedgen offers a quick system to generate traces based on a specific traffic format. We currently support a numbers of MPI collectives and also general traffic patters such as incast, permutation, tornado... Check the ```utilities/schedgen/schedgen.cpp``` file for more infromation.
As as example, to generate a permutation with multiple send/recv per node, we would use (after running ```make```):

```
./schedgen -p multiple_permutation -s 64 -o mp.goal -d 1000000
```
This will generate a ```.goal``` input file for 64 nodes and a message of 1MiB.

### Convert GOAL raw files to binary
Finally, to convert the GOAL input files to binary we need to use the built in tool. The usage is very simple and follows the following command line from ```utilities/goal_converter/``` (after running ```make``` from the same folder):
```
./txt2bin -i ./mp.goal -o ./mp.bin -p
```
This will generate a binary file that we can then use as input file for htsim when specifying the ```-goal``` parameter.

## Plotting

We currently provide several plotting files but the main is:

- ```performance_complete.py``` will generate a plot with some overall results including RTT, CWND, NACK, ECN, Queue sizes over time.

Please check the files for more command line options that can be used with both. It is also possible that some Python libraries will need to be installed
