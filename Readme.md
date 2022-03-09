# TDMH: A time deterministic wireless network stack

## Before you begin

This codebase has been tested only on Linux so far. It may or may not work under Windows.

More in detail, it relies on symbolic links and bash scripts.

### Setup Tools
Two main tools are needed:
- The [wandstem-flash-utility](https://github.com/legobrick/wandstem-flash-utility) for flashing a binary file to the WandStem nodes. `cmake` is needed to compile this tool.
- The [tdmh-visualizer](https://github.com/Gdvhev/tdmh_visualizer) which enriches the debugging experience by showing the network topology in real-time. To compile it, `qt5` and `qmake` are needed.

Make sure to have the required dependencies installed and then use the provided `scripts/install_tools.sh` script to correctly setup them.

### OMNeT++ Setup
- Download OMNeT++ 5.3 from [https://www.omnetpp.org/](www.omnetpp.org)
- Extract the downloaded file

Follow the instructions in the `doc/InstallGuide.pdf`, more in detail:

- Make sure you have the dependencies
```
sudo apt install build-essential gcc g++ bison flex perl \
qt5-default libqt5opengl5-dev tcl-dev tk-dev libxml2-dev \
zlib1g-dev default-jre doxygen graphviz libwebkitgtk-1.0
```
- Edit configure.user and set
```
WITH_OSG=no
WITH_OSGEARTH=no
```
- Enter the extracted folder an run `. setenv && ./configure && make -jN` with N number of your CPU cores
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

## Getting started with the simulator

Before being able to use the simulator you have to generate OMNeT++ makefiles and do a first build. DO NOT skip this step.

- Run the following commands
```
cd simulator/WandstemMac/src
opp_makemake -I. -lpthread -f --deep
cd ..
MODE=debug make
```

The simulator code is in the `simulator` directory, which contains

- `WandstemMac` which is the OMNeT++ simulation code. The 'main' of the root node and generic node is in the `src/RootNode.cpp` and `src/Node.cpp` files. The `src` directory contains the wrapper code for the Miosix OS API under OMNeT++, and a symbolic link to the network stack common code. `src/simulations` contains some example topologies.
- `simulation_scripts` contains automated simulation campaigns to measure e.g: network formation time as a function of the network size. These are bash scripts which run the OMNeT++ simulator repeatedly and parse results.
- `tools` are some scripts to automatically generate `.ned` files and parse results used by `simulation_scripts`

To run an example within the OMNeT++ IDE

- Double click on one of the topologies (*.ned files) from the Project window on the left,
they are located in `WandstemMac/simulations`
- Press on the green Play button (Run) on the upper toolbar
- When asked whether to switch to *Release Configuration*, say No
- Another window will open, you can run the simulation by using the "Start" buttons on the top left.


## Getting started with WandStem nodes

TODO

WandStem nodes webpage: [https://miosix.org/wandstem.html](https://miosix.org/wandstem.html)

### Flashing an executable
WandStem nodes have two buttons: one is used for resetting the board while the other one is a user programmable button.  
The WandStem user button is also needed for flashing an executable to the board: keep it pressed while powering up the device to make it entering the programming mode, then use the `wandstem-flash-utility` to flash it.

`wandstem-flash-utility -m u -f main.bin` can be used to flash the `main.bin` file using the USB mode. 

Otherwise, the TDMH makefile provides the `make program` command to achieve the same result.  
Command `make all program` can alternatively be used both for compiling the project and automatically flashing the resulting executable to the WandStem board.