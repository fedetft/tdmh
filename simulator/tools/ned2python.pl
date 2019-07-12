#!/usr/bin/perl

use warnings;
use strict;

print "topology = [";
my $first=1;
my $last=-1;
while(<>){
    next unless(/n(\d+)\.wireless\+\+ <--> n(\d+)\.wireless\+\+/);
    if($first) {
        $first=0;
        $last=$1;
    }
    if($1!=$last) {
        $last=$1;
        print "\n            ";
    }
    print "($1, $2), ";
}
print "]";

