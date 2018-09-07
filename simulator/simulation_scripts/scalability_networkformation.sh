#!/bin/bash

function fail {
    echo -e "\n\nFailed with error $1\n"
    exit 1
}

function noenv {
    echo -e "\nError: you do not have omnet++ environment variables set"
    echo -e "Try something like this"
    echo -e "\tcd <path where you have installed omnet++"
    echo -e "\t. ./setenv\n\tcd -"
    echo -e "Or put the setenv script of omnet++ in your bashrc"
    exit 1
}

which omnetpp > /dev/null || noenv;

# Compile the hexagonal topology generator
cd ../tools || fail
make || fail
cd -

# The perl scripts to measure convergence time rely on certain print_dbg,
# so to make the experiment reproducible, use the known good print_dbg
# settings
cmp debug_settings.h ../../network_module/debug_settings.h || \
        cp debug_settings.h ../../network_module

# Compile the omnet++ simulator
cd ../WandstemMac || fail
MODE=debug make || fail
cd -

# Parameters
# $1 number of nodes
# $2 max number of nodes
# $3 max number of hops
# $4 simulation time
# $5 results file
function hex_topology_experiment {
    echo "topology=hex node=$1 maxnodes=$2 maxhops=$3 simtime=$4"
    
    # NOTE: 0=hex
    ../../tools/nedgen_hexagon "Hex$1_$2" $1 $2 $3 $4 0 > /dev/null || fail
    
    ../../WandstemMac/out/clang-debug/src/WandstemMac_dbg -m -u Cmdenv  \
            -n .:../../WandstemMac/src "Hex$1_$2.ini"                   \
            --cmdenv-express-mode=false --cmdenv-log-prefix="%l %o %N:" \
            > /dev/null || fail
    
    perl ../../tools/postprocess.pl "results/Hex$1_$2.elog" || fail

    convergencetime=`perl ../../tools/convergencetime.pl "Hex$1_$2.ned" "results/Hex$1_$2.elog"`
    
    echo "$1 $2 $3 $convergencetime" >> "$5"
}

# Parameters
# $1 number of nodes
# $2 max number of nodes
# $3 max number of hops
# $4 simulation time
# $5 results file
function rhex_topology_experiment {
    echo "topology=rhex node=$1 maxnodes=$2 maxhops=$3 simtime=$4"
    
    # NOTE: 1=rhex
    ../../tools/nedgen_hexagon "RHex$1_$2" $1 $2 $3 $4 1 > /dev/null || fail
    
    ../../WandstemMac/out/clang-debug/src/WandstemMac_dbg -m -u Cmdenv  \
            -n .:../../WandstemMac/src "RHex$1_$2.ini"                   \
            --cmdenv-express-mode=false --cmdenv-log-prefix="%l %o %N:" \
            > /dev/null || fail
    
    perl ../../tools/postprocess.pl "results/RHex$1_$2.elog" || fail

    convergencetime=`perl ../../tools/convergencetime.pl "RHex$1_$2.ned" "results/RHex$1_$2.elog"`
    
    echo "$1 $2 $3 $convergencetime" >> "$5"
}

# Create the results and temporary directory

mkdir results_scalability_networkformation

rm -rf temp
mkdir temp || fail
cd temp

## Run the hex experiments

rm -f "../results_scalability_networkformation/hex_128.txt"
hex_topology_experiment   2 128 1 200 "../results_scalability_networkformation/hex_128.txt"
hex_topology_experiment   4 128 1 200 "../results_scalability_networkformation/hex_128.txt"
hex_topology_experiment   8 128 2 200 "../results_scalability_networkformation/hex_128.txt"
hex_topology_experiment  16 128 2 200 "../results_scalability_networkformation/hex_128.txt"
hex_topology_experiment  32 128 3 200 "../results_scalability_networkformation/hex_128.txt"
hex_topology_experiment  64 128 5 400 "../results_scalability_networkformation/hex_128.txt"
hex_topology_experiment 128 128 7 400 "../results_scalability_networkformation/hex_128.txt"

rm -f "../results_scalability_networkformation/hex_64.txt"
hex_topology_experiment   2  64 1 100 "../results_scalability_networkformation/hex_64.txt"
hex_topology_experiment   4  64 1 100 "../results_scalability_networkformation/hex_64.txt"
hex_topology_experiment   8  64 2 200 "../results_scalability_networkformation/hex_64.txt"
hex_topology_experiment  16  64 2 200 "../results_scalability_networkformation/hex_64.txt"
hex_topology_experiment  32  64 3 200 "../results_scalability_networkformation/hex_64.txt"
hex_topology_experiment  64  64 5 200 "../results_scalability_networkformation/hex_64.txt"

rm -f "../results_scalability_networkformation/hex_32.txt"
hex_topology_experiment   2  32 1 100 "../results_scalability_networkformation/hex_32.txt"
hex_topology_experiment   4  32 1 100 "../results_scalability_networkformation/hex_32.txt"
hex_topology_experiment   8  32 2 200 "../results_scalability_networkformation/hex_32.txt"
hex_topology_experiment  16  32 2 200 "../results_scalability_networkformation/hex_32.txt"
hex_topology_experiment  32  32 3 200 "../results_scalability_networkformation/hex_32.txt"

rm -f "../results_scalability_networkformation/hex_16.txt"
hex_topology_experiment   2  16 1 100 "../results_scalability_networkformation/hex_16.txt"
hex_topology_experiment   4  16 1 100 "../results_scalability_networkformation/hex_16.txt"
hex_topology_experiment   8  16 2 100 "../results_scalability_networkformation/hex_16.txt"
hex_topology_experiment  16  16 2 100 "../results_scalability_networkformation/hex_16.txt"

rm -f "../results_scalability_networkformation/hex_8.txt"
hex_topology_experiment   2   8 1 100 "../results_scalability_networkformation/hex_8.txt"
hex_topology_experiment   4   8 1 100 "../results_scalability_networkformation/hex_8.txt"
hex_topology_experiment   8   8 2 100 "../results_scalability_networkformation/hex_8.txt"

## Run the rhex experiments

rm -f "../results_scalability_networkformation/rhex_128.txt"
rhex_topology_experiment   2 128 1 200 "../results_scalability_networkformation/rhex_128.txt"
rhex_topology_experiment   4 128 1 200 "../results_scalability_networkformation/rhex_128.txt"
rhex_topology_experiment   8 128 2 200 "../results_scalability_networkformation/rhex_128.txt"
rhex_topology_experiment  16 128 2 200 "../results_scalability_networkformation/rhex_128.txt"
rhex_topology_experiment  32 128 3 200 "../results_scalability_networkformation/rhex_128.txt"
rhex_topology_experiment  64 128 5 400 "../results_scalability_networkformation/rhex_128.txt"
rhex_topology_experiment 128 128 7 400 "../results_scalability_networkformation/rhex_128.txt"

rm -f "../results_scalability_networkformation/rhex_64.txt"
rhex_topology_experiment   2  64 1 100 "../results_scalability_networkformation/rhex_64.txt"
rhex_topology_experiment   4  64 1 100 "../results_scalability_networkformation/rhex_64.txt"
rhex_topology_experiment   8  64 2 200 "../results_scalability_networkformation/rhex_64.txt"
rhex_topology_experiment  16  64 2 200 "../results_scalability_networkformation/rhex_64.txt"
rhex_topology_experiment  32  64 3 200 "../results_scalability_networkformation/rhex_64.txt"
rhex_topology_experiment  64  64 5 200 "../results_scalability_networkformation/rhex_64.txt"

rm -f "../results_scalability_networkformation/rhex_32.txt"
rhex_topology_experiment   2  32 1 100 "../results_scalability_networkformation/rhex_32.txt"
rhex_topology_experiment   4  32 1 100 "../results_scalability_networkformation/rhex_32.txt"
rhex_topology_experiment   8  32 2 200 "../results_scalability_networkformation/rhex_32.txt"
rhex_topology_experiment  16  32 2 200 "../results_scalability_networkformation/rhex_32.txt"
rhex_topology_experiment  32  32 3 200 "../results_scalability_networkformation/rhex_32.txt"

rm -f "../results_scalability_networkformation/rhex_16.txt"
rhex_topology_experiment   2  16 1 100 "../results_scalability_networkformation/rhex_16.txt"
rhex_topology_experiment   4  16 1 100 "../results_scalability_networkformation/rhex_16.txt"
rhex_topology_experiment   8  16 2 100 "../results_scalability_networkformation/rhex_16.txt"
rhex_topology_experiment  16  16 2 100 "../results_scalability_networkformation/rhex_16.txt"

rm -f "../results_scalability_networkformation/rhex_8.txt"
rhex_topology_experiment   2   8 1 100 "../results_scalability_networkformation/rhex_8.txt"
rhex_topology_experiment   4   8 1 100 "../results_scalability_networkformation/rhex_8.txt"
rhex_topology_experiment   8   8 2 100 "../results_scalability_networkformation/rhex_8.txt"

cd ..

scilab -f plot_scalability_networkformation.sce || echo "Scilab not found, not plotting results"
