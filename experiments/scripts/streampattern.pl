#!/usr/bin/perl

use warnings;
use strict;

die "use: perl streampattern.pl streamsrc streamdst < logfile" unless($#ARGV+1==2);
my $streamsrc = $ARGV[0];
my $streamdst = $ARGV[1];

my $receivedCount = 0;
my $missedCount   = 0;

while(<STDIN>)
{
    if(/\[A\] Received data from \((\d+),(\d+)\)/ ||
       /\[A\] R \((\d+),(\d+)\)/) {
        my $src=$1, my $dst=$2;
        if ($src==$streamsrc && $dst==$streamdst) {
            print "1";
            $receivedCount++;
        }

    } elsif(/\[E\] No data received from Stream \((\d+),(\d+)\)/ ||
            /\[E\] M \((\d+),(\d+)\)/) {
        my $src=$1, my $dst=$2;
        if ($src==$streamsrc && $dst==$streamdst) {
            print "0";
            $missedCount++;
        }
    }
}

print "\n\n";
print "Stream [$streamsrc-$streamdst]: RECEIVED=${receivedCount} MISSED=${missedCount}\n";
print "\n";
