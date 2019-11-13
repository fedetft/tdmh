#!/usr/bin/perl

use warnings;
use strict;

die "use: perl directneighbor-rssi.pl node < logfile" unless($#ARGV+1==1);
my $node=$ARGV[0];

my $line=0;

while(<STDIN>){
    $line++;
    next unless(/\[U\]<-N=(\d+) @\d+ (-?\d+)dBm/);
    next unless($1==$node);
    print "$line $2\n";
}
