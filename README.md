# SPCL htsim

This repository is a SPCL fork of the official UEC repository. We plan to support these features on top of the UE code:

- ATLAHS integration to run GOAL files.
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