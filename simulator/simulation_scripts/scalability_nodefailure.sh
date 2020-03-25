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
cmp debug_settings.h ../../network_module/util/debug_settings.h || \
        cp debug_settings.h ../../network_module/util

# Compile the omnet++ simulator
cd ../WandstemMac || fail
MODE=debug make || fail
cd -

# Parameters
# $1 number of nodes
# $2 max number of nodes
# $3 max number of hops
# $4 simulation time
# $5 connect|disconnect
# $6 results file
function hex_topology_experiment {
    echo "topology=hex node=$1 maxnodes=$2 maxhops=$3 simtime=$4 $5"
    
    # Hardcoding connect/disconnect time to half simulation time
    time=$(($4 / 2))
    node=$(($1 - 1))
    connstr="$node $5 $time"
    
    # NOTE: 0=hex
    ../../tools/nedgen_hexagon "Hex$1_$2" $1 $2 $3 $4 0 "$connstr" > /dev/null || fail
    
    ../../WandstemMac/out/clang-debug/src/WandstemMac_dbg -m -u Cmdenv  \
            -n .:../../WandstemMac/src "Hex$1_$2.ini"                   \
            --cmdenv-express-mode=false --cmdenv-log-prefix="%l %o %N:" \
            > /dev/null || fail
    
    perl ../../tools/postprocess.pl "results/Hex$1_$2.elog" || fail

    convergencetime=`perl ../../tools/disconnecttime.pl "Hex$1_$2.ned" "results/Hex$1_$2.elog"`
    
    echo "$1 $2 $3 $convergencetime" >> "$6"
}

# Parameters
# $1 number of nodes
# $2 max number of nodes
# $3 max number of hops
# $4 simulation time
# $5 connect|disconnect
# $6 results file
function rhex_topology_experiment {
    echo "topology=rhex node=$1 maxnodes=$2 maxhops=$3 simtime=$4 $5"
    
    # Hardcoding connect/disconnect time to half simulation time
    time=$(($4 / 2))
    node=1
    connstr="$node $5 $time"
    
    # NOTE: 1=rhex
    ../../tools/nedgen_hexagon "RHex$1_$2" $1 $2 $3 $4 1 "$connstr" > /dev/null || fail
    
    ../../WandstemMac/out/clang-debug/src/WandstemMac_dbg -m -u Cmdenv  \
            -n .:../../WandstemMac/src "RHex$1_$2.ini"                  \
            --cmdenv-express-mode=false --cmdenv-log-prefix="%l %o %N:" \
            > /dev/null || fail
    
    perl ../../tools/postprocess.pl "results/RHex$1_$2.elog" || fail

    convergencetime=`perl ../../tools/disconnecttime.pl "RHex$1_$2.ned" "results/RHex$1_$2.elog"`
    
    echo "$1 $2 $3 $convergencetime" >> "$6"
}

# Create the results and temporary directory

mkdir results_scalability_nodefailure

rm -rf temp
mkdir temp || fail
cd temp

## Run the hex experiments for node disconnect

rm -f "../results_scalability_nodefailure/hex_128.txt"
hex_topology_experiment   2 128 1 400 disconnect "../results_scalability_nodefailure/hex_128.txt"
hex_topology_experiment   4 128 1 400 disconnect "../results_scalability_nodefailure/hex_128.txt"
hex_topology_experiment   8 128 2 400 disconnect "../results_scalability_nodefailure/hex_128.txt"
hex_topology_experiment  16 128 2 400 disconnect "../results_scalability_nodefailure/hex_128.txt"
hex_topology_experiment  32 128 3 400 disconnect "../results_scalability_nodefailure/hex_128.txt"
hex_topology_experiment  64 128 5 800 disconnect "../results_scalability_nodefailure/hex_128.txt"
hex_topology_experiment 128 128 7 800 disconnect "../results_scalability_nodefailure/hex_128.txt"

rm -f "../results_scalability_nodefailure/hex_64.txt"
hex_topology_experiment   2  64 1 200 disconnect "../results_scalability_nodefailure/hex_64.txt"
hex_topology_experiment   4  64 1 200 disconnect "../results_scalability_nodefailure/hex_64.txt"
hex_topology_experiment   8  64 2 400 disconnect "../results_scalability_nodefailure/hex_64.txt"
hex_topology_experiment  16  64 2 400 disconnect "../results_scalability_nodefailure/hex_64.txt"
hex_topology_experiment  32  64 3 400 disconnect "../results_scalability_nodefailure/hex_64.txt"
hex_topology_experiment  64  64 5 400 disconnect "../results_scalability_nodefailure/hex_64.txt"

rm -f "../results_scalability_nodefailure/hex_32.txt"
hex_topology_experiment   2  32 1 200 disconnect "../results_scalability_nodefailure/hex_32.txt"
hex_topology_experiment   4  32 1 200 disconnect "../results_scalability_nodefailure/hex_32.txt"
hex_topology_experiment   8  32 2 400 disconnect "../results_scalability_nodefailure/hex_32.txt"
hex_topology_experiment  16  32 2 400 disconnect "../results_scalability_nodefailure/hex_32.txt"
hex_topology_experiment  32  32 3 400 disconnect "../results_scalability_nodefailure/hex_32.txt"

rm -f "../results_scalability_nodefailure/hex_16.txt"
hex_topology_experiment   2  16 1 200 disconnect "../results_scalability_nodefailure/hex_16.txt"
hex_topology_experiment   4  16 1 200 disconnect "../results_scalability_nodefailure/hex_16.txt"
hex_topology_experiment   8  16 2 200 disconnect "../results_scalability_nodefailure/hex_16.txt"
hex_topology_experiment  16  16 2 200 disconnect "../results_scalability_nodefailure/hex_16.txt"

rm -f "../results_scalability_nodefailure/hex_8.txt"
hex_topology_experiment   2   8 1 200 disconnect "../results_scalability_nodefailure/hex_8.txt"
hex_topology_experiment   4   8 1 200 disconnect "../results_scalability_nodefailure/hex_8.txt"
hex_topology_experiment   8   8 2 200 disconnect "../results_scalability_nodefailure/hex_8.txt"

## Run the rhex experiments

rm -f "../results_scalability_nodefailure/rhex_128.txt"
rhex_topology_experiment   2 128 1 400 disconnect "../results_scalability_nodefailure/rhex_128.txt"
rhex_topology_experiment   4 128 1 400 disconnect "../results_scalability_nodefailure/rhex_128.txt"
rhex_topology_experiment   8 128 2 400 disconnect "../results_scalability_nodefailure/rhex_128.txt"
rhex_topology_experiment  16 128 2 400 disconnect "../results_scalability_nodefailure/rhex_128.txt"
rhex_topology_experiment  32 128 3 400 disconnect "../results_scalability_nodefailure/rhex_128.txt"
rhex_topology_experiment  64 128 5 800 disconnect "../results_scalability_nodefailure/rhex_128.txt"
rhex_topology_experiment 128 128 7 800 disconnect "../results_scalability_nodefailure/rhex_128.txt"

rm -f "../results_scalability_nodefailure/rhex_64.txt"
rhex_topology_experiment   2  64 1 200 disconnect "../results_scalability_nodefailure/rhex_64.txt"
rhex_topology_experiment   4  64 1 200 disconnect "../results_scalability_nodefailure/rhex_64.txt"
rhex_topology_experiment   8  64 2 400 disconnect "../results_scalability_nodefailure/rhex_64.txt"
rhex_topology_experiment  16  64 2 400 disconnect "../results_scalability_nodefailure/rhex_64.txt"
rhex_topology_experiment  32  64 3 400 disconnect "../results_scalability_nodefailure/rhex_64.txt"
rhex_topology_experiment  64  64 5 400 disconnect "../results_scalability_nodefailure/rhex_64.txt"

rm -f "../results_scalability_nodefailure/rhex_32.txt"
rhex_topology_experiment   2  32 1 200 disconnect "../results_scalability_nodefailure/rhex_32.txt"
rhex_topology_experiment   4  32 1 200 disconnect "../results_scalability_nodefailure/rhex_32.txt"
rhex_topology_experiment   8  32 2 400 disconnect "../results_scalability_nodefailure/rhex_32.txt"
rhex_topology_experiment  16  32 2 400 disconnect "../results_scalability_nodefailure/rhex_32.txt"
rhex_topology_experiment  32  32 3 400 disconnect "../results_scalability_nodefailure/rhex_32.txt"

rm -f "../results_scalability_nodefailure/rhex_16.txt"
rhex_topology_experiment   2  16 1 200 disconnect "../results_scalability_nodefailure/rhex_16.txt"
rhex_topology_experiment   4  16 1 200 disconnect "../results_scalability_nodefailure/rhex_16.txt"
rhex_topology_experiment   8  16 2 200 disconnect "../results_scalability_nodefailure/rhex_16.txt"
rhex_topology_experiment  16  16 2 200 disconnect "../results_scalability_nodefailure/rhex_16.txt"

rm -f "../results_scalability_nodefailure/rhex_8.txt"
rhex_topology_experiment   2   8 1 200 disconnect "../results_scalability_nodefailure/rhex_8.txt"
rhex_topology_experiment   4   8 1 200 disconnect "../results_scalability_nodefailure/rhex_8.txt"
rhex_topology_experiment   8   8 2 200 disconnect "../results_scalability_nodefailure/rhex_8.txt"

cd ..

scilab -f plot_scalability_nodefailure.sce || echo "Scilab not found, not plotting results"
