#!/bin/bash

#
# TODO
#
# check if specific parameter exists
#
# if [[ "$*" == *YOURSTRING* ]]
# then
#     echo "YES"
# else
#     echo "NO"
# fi

printUsage() {
    echo "Usage: ./latency_stats.sh logfile [--plot [streamsrc streamdst]] [--export]"
    echo "Args:"
    echo $'\t--plot \t\toptional, produce latency plots, not only statistics, optionally specify source and destination nodes.'
    echo $'\t\t\t- streamsrc: source node ID, integer'
    echo $'\t\t\t- streamdst: destination node ID, integer'
    echo $'\t\t\t- If streamsrc and streamdst not specified, all combinations are tried'
    echo $'\t--export \toptional, export results and plots to file'
    echo $'\t-h | --help \tprint this help message'
    echo $'\n\tStatistics for all streams are always output'
    exit 1
}

function computeStats {
    # function parameters
    logFile=$1
    # source and destination nodes
    statsFile=$scriptDirectory/results/latency_stats.txt

    # produce statistics, (max, min, avg, stddev)
    # - script arguments are src and dst nodes for required stream
    # - input file passed from stdin
    # - output to statsFile if required
    if [ $exportResults == true ]; then
        echo -n $'\nExporting statistics'
        perl $scriptDirectory/latency_stats.pl --stats < $logFile > $statsFile
        echo $'... done'
    else
        perl $scriptDirectory/latency_stats.pl --stats < $logFile
    fi
}

function latencyPlot {
    # function parameters
    logFile=$1
    # source and destination nodes
    src=$2
    dst=$3

    latencyFile=$scriptDirectory/latency_${src}_${dst}.txt
    plotFile=$scriptDirectory/results/latency_plot_${src}_${dst}.pdf

    # compute .txt file with data for plots
    perl $scriptDirectory/latency_stats.pl --plot $src $dst < $logFile > $latencyFile

    if [ -f $latencyFile ]; then        # latency file exists
        if [ -s $latencyFile ]; then    # latency file size > 0 (not empty)
            
            echo "Plotting Stream ($src,$dst)"

            # produce plot
            if [ $exportResults == true ]; then
                echo -n "Exporting plot for Stream ($src,$dst)"
                scilab -f $scriptDirectory/latency_plots.sce -args $latencyFile true $plotFile -nw # avoid scilab complete gui
                echo $'... done\n'
            else 
                scilab -f $scriptDirectory/latency_plots.sce -args $latencyFile false
            fi
        else
            echo "No data for Stream ($src,$dst)"
        fi
        
        rm $latencyFile # remove the temporary file for plotting
    
    else
        echo "Latency file does not exist for Stream ($src,$dst)"
    fi
}

#
#   PARSE ARGUMENTS
#

if [[ $# < 1 ]]; then # at least 1 param
    #echo $'\nInvalid number of parameters, minimum 2 are needed'
    printUsage
fi

if [[ $1 == "-h" || $1 == "--help" ]]; then 
    printUsage
fi

if [[ ! -f "$1" ]]; then # if log file exists
    echo $'\nInvalid log file\n'
    printUsage
fi

# script directory
scriptDirectory="$(dirname "$0")"

# source and destination nodes
src=0
dst=0

exportFlag="--export"
plotFlag="--plot"

if [[ "$*" == $exportFlag ]]
then
    mkdir -p $scriptDirectory/results # create results directory if doesn't exist
fi 

exportResults=false
if [[ $# -eq 2 || $# -eq 3 || $# -eq 5 ]]; then # --export given as param 2, 3 or 5 (if --plot or --plot src dst specified)
    if [[ $2 == $exportFlag || $3 == $exportFlag || $5 == $exportFlag ]]; then # export plot to pdf and results to txt
        exportResults=true
    # else
    #     #echo $'\nInvalid parameter given'
    #     printUsage
    fi
fi

producePlots=false
plotAll=false
if [[ $# -eq 2 || $# -eq 3 || $# -eq 4 || $# -eq 5 ]]; then # --plot given as param 2 or 3 (if also --export specified)
    if [[ $2 == $plotFlag || $3 == $plotFlag ]]; then
        producePlots=true
        if [[ $# > 3 && $3 != $exportFlag && $4 != $exportFlag ]]; then # src and dst specified
            if [[ $2 == $plotFlag ]]; then
                src=$3
                dst=$4
            elif [[ $3 == $plotFlag ]]; then
                src=$4
                dst=$5
            fi
        else # src and dst not specified
            plotAll=true
        fi
    # else
    #     #echo $'\nInvalid parameter given'
    #     printUsage
    fi
fi

if [[ $# -eq 2 ]]; then
    if [[ $2 != $plotFlag && $2 != $exportFlag ]]; then
        printUsage
    fi 
fi

#
#   MAIN
#

# first of all print stats for all streams
echo $'Computing statistics for all streams...'
computeStats $1 # param = log file

# if needed also produce plots
if [ $producePlots == true ]; then

    echo $'\nStarting plots\n'

    if [ $plotAll == true ]; then
        echo $'Plot for all nodes\n'
        # all combinations of source and destination nodes
        for i in {0..14}
        do
            for j in {0..14}
            do
                latencyPlot $1 $i $j # log file, sender node, receiver node
            done
        done
    else
        latencyPlot $1 $src $dst # log file, sender node, receiver node ---> script params, src and dst
    fi
fi