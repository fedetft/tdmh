#!/usr/bin/perl

use warnings;
use strict;

my $line=0;
my $printtopo=0;
my %oldtopo;
my %newtopo;

while(<STDIN>)
{
    $line++;
    
    if($printtopo)
    {
        if(/\[SC\] End Topology/) {
            print "$line ===\n";
            foreach(sort keys %newtopo) {
                print "+ $_\n" unless($oldtopo{$_});
            }
            foreach(sort keys %oldtopo) {
                print "- $_\n" unless($newtopo{$_});
            }
            
            %oldtopo=%newtopo;
            %newtopo=();
            $printtopo=0;
        } elsif(/(\[\d+ - \d+\])/) {
            $newtopo{$1}=1;
        }
    } else {
        if(/\[SC\] Begin Topology/)
        {
            $printtopo=1;
        }
    }
}
