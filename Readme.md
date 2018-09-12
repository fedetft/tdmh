# TDMH: A time deterministic wireless network stack

## Before you begin

This codebase has been tested only on Linux so far. It may or may not work under Windows.

More in detail, it relies on symbolic links and bash scripts.

Currently, the OMNeT++ simulator only works in debug mode, this will be fixed soon.

## Getting started with the simulator

The simulator code is the `simulator` directory, which contains

- `WandstemMac` which is the OMNeT++ simulation code. The 'main' of the root node and generic node is in the `src/RootNode.cpp` and `src/Node.cpp` files. The `src` directory contains the wrapper code for the Miosix OS API under OMNeT++, and a link to the network stack. `src/simulations` contains some example topologies.
- `simulation_scripts` contains automated simulation campaigns to measure e.g: network formation time as a function of the network size. These are bash scripts which run the OMNeT++ simulator repeatedly and parse results.
- `tools` are some scripts to automatically generate `.ned` files and parse results used by `simulation_scripts`

## Getting started with Wandstem nodes

TODO
