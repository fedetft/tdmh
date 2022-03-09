#!/bin/sh
#
# Install utilities to flash WandStem nodes
# and to visualize the network topology
#
# Resulting paths are:
# - tdmh/scripts/wandstem-flash-utility/build/wandstem-flash
# - tdmh/experiments/scripts/tdmh_visualizer/visualizer
#

scriptDirectory="$(dirname "$0")"
numCpus=$(grep -c ^processor /proc/cpuinfo)

cd $scriptDirectory

printf "Installing WandStem nodes flash utility\n"
git clone --recurse-submodule https://github.com/legobrick/wandstem-flash-utility.git
mkdir -p wandstem-flash-utility/build # create build folder if does not exists
cd wandstem-flash-utility/build
cmake ..
make -j $numCpus
cd ../..

printf "\nInstalling TDMH network topology visualizer\n"
cd ../experiments/scripts
echo $(pwd)
git clone https://github.com/Gdvhev/tdmh_visualizer.git
cd tdmh_visualizer
./genMakefile.sh
cd build.Release
make -j $numCpus
mv src/src ../visualizer # rename generated executable