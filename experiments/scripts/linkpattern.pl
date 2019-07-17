#!/usr/bin/perl

use warnings;
use strict;

die "use: perl linkpattern.pl streamsrc streamdst < logfile" unless($#ARGV+1==2);
my $streamsrc=$ARGV[0];
my $streamdst=$ARGV[1];

print "stream [$streamsrc-$streamdst]\n";

my $enable=0;
my $found=0;
my $prev=-1;
my $changes=0;
my $schedules=0;
while(<STDIN>)
{
    if(/\[SC\] Begin Topology/) {
        $enable=1;
        $schedules++;

    } elsif(/\[(\d+) - (\d+)\]/) {
        my $src=$1, my $dst=$2;
        $found=1 if $src==$streamsrc && $dst==$streamdst;

    } elsif(/\[SC\] End Topology/) {
        if($found) { print "1"; } else { print "0"; }
        if($prev!=$found) {
            $prev=$found;
            $changes++;
        }
        $enable=0;
        $found=0;
    }
}

print "\n\nNumber of schedules $schedules\nTotal topology changes $changes\n";
