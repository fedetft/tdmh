#!/usr/bin/perl

use warnings;
use strict;

my %neighbors;

while(<>) {
    next unless(/\[U\]<-N=(\d+) @\d+ -?\d+dBm/);
    $neighbors{$1}=1;
}

foreach(sort keys %neighbors) {
    print "$_\n";
}
