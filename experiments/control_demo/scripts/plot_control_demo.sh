#!/bin/bash

fail() {
    echo "use: ./parse_log.sh logfile"
    exit 1
}

if [[ $# -ne 1 ]]; then 
    echo "Invalid number of parameters"
    fail
fi

logfile=$1

if [[ ! -f "$logfile" ]]; then
    echo "Invalid log file"
    fail
fi

scriptDirectory="$(dirname "$0")"
plotInputfile="control_fornace.txt"

perl -e 'my $c=0;while(<STDIN>) {next unless /\[A\] S=(\d+) CV=(\d+) \(= (\d+).(\d+) %\)/; $c++; print "$c $1 $2 $3.$4\n";}' < $logfile > $scriptDirectory/$plotInputfile

scilab -f $scriptDirectory/plot_control_demo.sce -args $scriptDirectory/$plotInputfile

rm $scriptDirectory/$plotInputfile