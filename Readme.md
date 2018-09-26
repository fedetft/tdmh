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

### OMNeT++ Setup
- Download OMNeT++ 5.3 from [https://www.omnetpp.org/](www.omnetpp.org)
- Exctract the downloaded file
- Enter the extracted folder an run `.configure && make -jN` with N number of CPU cores
- Add the following line to your `~/.profile`
```
export PATH=$PATH:/<where you extracted the archive>/omnetpp-5.3/bin
```
- Source the updated profile with `source ~/.profile` or logout and login
- Now you should be able to launch the simulator with the `omnetpp` command
- At the first launch, the simulator will ask you for a working directory, 
choose `tdmh/simulator/`
- Close the Welcome window
- You will be asked to install the INET framework or OMNeT++ programming examples,
do not install those as they aren't used in this project.
- Now select File->Open Project from File System, and select the following folder
by pressing the "Directory" button
`tdmh/simulator/WandstemMac`
- Now you can close the "Embedded browser" window
### OMNeT++ usage
- Double click on one of the topologies (*.ned files) from the Project window on the left,
they are located in `WandstemMac/simulations`
- Press on the green Play button (Run) on the upper toolbar
- When asked whether to switch to *Release Configuration*, say No
- Another window will open, you can run the simulation by using the "Start" buttons on the top left.

## Getting started with Wandstem nodes

TODO
