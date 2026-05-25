# SPCL htsim

This repository is a SPCL fork of the official UEC repository. We plan to support these features on top of the UE code:

- ATLAHS integration to run GOAL files.
- Custom network topologies: Dragonfly and SlimFly (in addition to Fat Tree).
- Implementation of REPS and SMaRTT.
- Implementation of PCM logic (in progress).
- Loss Recovery algorithms such as PFLD.
- General htsim performance improvements.
- Ability to run datacenter traces.

# ATLAHS Integration
This repository also supports [ATLAHS](https://arxiv.org/abs/2505.08936) integration to run GOAL files in htsim. 

A number of already collected traces can be downloaded from [here](http://storage2.spcl.ethz.ch/traces/).

If you want more information on ATLAHS and details about how to collect traces, you can go to the [official repository](https://github.com/spcl/atlahs).

To do so, users can specify the GOAL file simply using the ```-goal``` option instead of the typical ```-cm``` option used when running connection matrices.

For example, one run can be done using:
```
./htsim_uec -goal atlahs_input/llama_N4_GPU16.bin -sender_cc_only -nodes 1024 -end 10000000 -topo topologies/fat_tree_1024_1os.topo -linkspeed 200000 > output.tmp
```

# Ongoing Development
We note that this repository is an ongoing development and more features and documentation will be added in the near future. Please feel free to report issues or problems using GitHub issues.

# Purpose and Scope

HTSIM is a high-performance discrete event simulator used for network simulation. 
It offers faster simulation methods compared to other options, making it ideal for modeling and developing congestion algorithms and new network protocols.
The role of htsim in the Ultra Ethernet Consortium (UEC) standards development is to support the transport layer working group's work on congestion control mechanisms.

In UEC, htsim:

- provides a platform for continuous implementation and development of UEC transport layer.
- is used to simulate and run different topologies and scenarios, helping to identify issues in the current specifications and estimate the throughput and latency for given parameters like topology, flow matrix and congestion configuration.
- provides a reference for users and developers to run simulations with different configurable parameters for various scenarios and algorithms


htsim's role is deliberately focused on congestion control.

UEC's htsim is not:

- a complete implementation of the UEC transport specification.
- a standard in any way; specifically, it is not part of the official UEC standards release.
  While we aim to match the spec as closely as possible, there might be discrepancies between the UEC CMS specification and the simulator.
  Only the official CMS specification is significant, the simulator is not.


# More insutrctions about htsim

Check the [README](htsim/README.md) file in the `htsim/` folder.

# Supported Topologies

All topologies are accessible through the unified `htsim_uec` binary. Use the `-topology` flag to select the topology type.

### Fat Tree (default)

The default topology. Specify a `.topo` file with `-topo`, or omit it to use a default 3-tier fat tree.

```bash
./htsim_uec -topo htsim/sim/datacenter/topologies/fat_tree_1024_1os.topo -tm connection_matrices/incast_2-1.tm
```

### Dragonfly

Uses `-topo` to point to a topology directory containing `dragonfly.topo`, `dragonfly.adjlist`, and a `host_table/` folder. Pre-generated assets are in `topologies/dragonfly/` (e.g., `p3a6h3`, `p4a8h4`).

Routing strategies: `MINIMAL`, `VALIANT`, `UGAL_L`, `SOURCE`

```bash
./htsim_uec -topology dragonfly -topo htsim/sim/datacenter/topologies/dragonfly/p3a6h3 -tm traffic.tm -routing MINIMAL -q 88
```

### SlimFly

Uses `-topo` to point to a topology directory containing `slimfly.topo`, `slimfly.adjlist`, and a `host_table/` folder. Pre-generated assets are in `topologies/slimfly/` (e.g., `p4q5`, `p7q9`).

Routing strategies: `MINIMAL`, `VALIANT`, `UGAL_L`, `SOURCE`

```bash
./htsim_uec -topology slimfly -topo htsim/sim/datacenter/topologies/slimfly/p4q5 -tm traffic.tm -routing MINIMAL -q 88
```

### Traffic Matrix Format

All topologies use the same traffic matrix format:

```
Nodes <N>
Connections <M>
<src>-><dst> start <picoseconds> size <bytes>
```

For more details, see [htsim/README.md](htsim/README.md).

## Spritz Integration

This branch integrates the Spritz source-routing load balancers from the `ad` branch of `https://github.com/aleskubicek/sc25-spritz` into the Dragonfly and SlimFly UEC binaries. With `-routing SOURCE`, use `-LB ECMP`, `OPS`, `FLICR`, `FLOW_V1` for Spritz-Scout, or `FLOW_V2` for Spritz-Spray. The Spritz artifact workloads, topology assets, batch scripts, and reproduction drivers are under [htsim/sim/datacenter](htsim/sim/datacenter).

To reproduce the compact Dragonfly comparison:

```bash
cd htsim/sim
cmake -S . -B build
cmake --build build --parallel
cd datacenter
python3 reproduce_spritz_subset.py
```

The resulting CSV is written to `experiments_output/spritz_subset/p4a8h4/permutation_global_4MiB/summary.csv`. See [htsim/README.md](htsim/README.md) for the full Spritz flag list and artifact script commands.

The full `ad` artifact pipeline is available via:

```bash
bash reproduce.sh quick
bash reproduce.sh full
```

Paper-style plots are written under `paper_plots/`. For example:

```bash
bash reproduce.sh plot fig6 quick
cd htsim/sim/datacenter
python3 simulate_df_no_fail.py --output-root experiments_output_quick --only-experiment adv_i5_4MiB --parallel 4
OUTPUT_ROOT=experiments_output_quick OUT_DIR=../../../paper_plots/quick/fig1 bash run_fig_1.sh
```


# References
If you use ATLAHS for your research, please cite our paper using:
```
@misc{shen2025atlahsapplicationcentricnetworksimulator,
      title={ATLAHS: An Application-centric Network Simulator Toolchain for AI, HPC, and Distributed Storage}, 
      author={Siyuan Shen and Tommaso Bonato and Zhiyi Hu and Pasquale Jordan and Tiancheng Chen and Torsten Hoefler},
      year={2025},
      eprint={2505.08936},
      archivePrefix={arXiv},
      primaryClass={cs.DC},
      url={https://arxiv.org/abs/2505.08936}, 
}
```
