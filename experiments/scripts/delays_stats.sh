#!/bin/bash

fail() {
    echo "use: ./delay_stats.sh logfile src dst"
    exit 1
}

if [[ $# -ne 3 ]]; then 
    echo "Invalid number of parameters"
    fail
fi
if [[ ! -f "$1" ]]; then
    echo "Invalid log file"
    fail
fi

perl -e 'my ($src,$dst)=@ARGV;my $l=0;while(<STDIN>){$l++;next unless /Delay \($src,$dst\): D=(\d+)/; print "$l $1\n";}' $2 $3 < $1 > delays.txt
perl -e 'my ($src,$dst)=@ARGV;my $l=0;while(<STDIN>){$l++;next unless /Mean Delay \($src,$dst\): MD=(\d+)/; print "$l $1\n";}' $2 $3 < $1 > delays_mean.txt
perl -e 'my ($src,$dst)=@ARGV;my $l=0;while(<STDIN>){$l++;next unless /Delay Standard Deviation \($src,$dst\): DV=(\d+)/; print "$l $1\n";}' $2 $3 < $1 > delays_stddev.txt

scilab -f "$(dirname "$0")"/delays_stats.sce

rm delays.txt delays_mean.txt delays_stddev.txt
