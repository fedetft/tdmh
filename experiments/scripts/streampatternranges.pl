#!/usr/bin/perl

use warnings;
use strict;

die "use: perl streampatternranges.pl streamsrc streamdst < logfile" unless($#ARGV+1==2);
my $streamsrc=$ARGV[0];
my $streamdst=$ARGV[1];
my $lineno=0;
my $begin=-1;
my $end;

print "stream [$streamsrc-$streamdst]\n";

while(<STDIN>)
{
    $lineno++;
    if(/\[A\] Received data from \((\d+),(\d+)\)/ ||
       /\[A\] R \((\d+),(\d+)\)/) {
        my $src=$1, my $dst=$2;
        if($src==$streamsrc && $dst==$streamdst)
        {
            if($begin>0)
            {
                print "$begin - $end\n";
                $begin=-1;
            }
        }

    } elsif(/\[E\] No data received from Stream \((\d+),(\d+)\)/ ||
            /\[E\] M \((\d+),(\d+)\)/) {
        my $src=$1, my $dst=$2;
        if($src==$streamsrc && $dst==$streamdst)
        {
            $begin=$lineno if($begin==-1);
            $end=$lineno;
        }
    }
}

print "\n";
