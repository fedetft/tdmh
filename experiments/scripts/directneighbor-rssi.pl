#!/usr/bin/perl

use warnings;
use strict;

my $numargs=$#ARGV+1;
die "use: perl directneighbor-rssi.pl node <uplinkperiod> < logfile"
    unless($numargs==1 || $numargs==2);
my $node=$ARGV[0];
my $period=-1;
$period=$ARGV[1] if($numargs==2);

my $previous=-1;
my $slack=0.0001;

while(<STDIN>){
    next unless(/\[U\]<-N=(\d+) @(\d+) (-?\d+)dBm/);
    next unless($1==$node);
    my $time=$2/1000000000.0;
    my $rssi=$3;
    if($period>0 && $previous>0) {
        # If we are given the uplink period, print Nan if uplink packets are missed
        while($previous+$period+$slack<$time) {
            $previous+=$period;
            printf("%.2f,Nan\n",$previous);
        }
    }
    printf("%.2f,%d\n",$time,$rssi);
    $previous=$time;
}
