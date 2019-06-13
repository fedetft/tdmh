#!/usr/bin/perl

use warnings;
use strict;

die "use: perl streampattern.pl streamsrc streamdst < logfile" unless($#ARGV+1==2);
my $streamsrc=$ARGV[0];
my $streamdst=$ARGV[1];

print "stream [$streamsrc-$streamdst]\n";

while(<STDIN>)
{
    if(/\[A\] Received data from \((\d+),(\d+)\)/) {
        my $src=$1, my $dst=$2;
        print "1" if $src==$streamsrc && $dst==$streamdst;

    } elsif(/\[E\] No data received from Stream \((\d+),(\d+)\)/) {
        my $src=$1, my $dst=$2;
        print "0" if $src==$streamsrc && $dst==$streamdst;
    }
}

print "\n";
