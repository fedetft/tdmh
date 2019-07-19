#!/usr/bin/perl

use warnings;
use strict;

my $line=0;
my %streams;

while(<STDIN>)
{
    $line++;
    if(/\[D\] Node 0: Received packet for stream \((\d+),(\d+)\)/ ||
       /\[D\] r \((\d+),(\d+)\)/) {
        my $src=$1, my $dst=$2;

        my $key="$src-$dst";
        if($streams{$key}) {
            my ($sp, $rs, $rd, $rt, $pha, $f, $s, $t) = @{$streams{$key}};
            $pha++;
            if($pha == 1)    { $f=1; }
            elsif($pha == 2) { $s=1; }
            elsif($pha == 3) { $t=1; }
            else { print "Note @ line $line: $key out of phase $pha\n"; }
            $streams{$key} = [ $sp, $rs, $rd, $rt, $pha, $f, $s, $t ];

        } else {
            $streams{$key} =
                [
                    0, # Sent packets
                    0, # Received with single redundancy
                    0, # Received with double redundancy
                    0, # Received with triple redundancy
                    1, # Phase in redundancy group
                    1, # Received first
                    0, # Received second
                    0, # Received third
                ];
        }

    } elsif(/\[D\] Node 0: Missed packet for stream \((\d+),(\d+)\)/ ||
            /\[D\] m \((\d+),(\d+)\)/) {
        my $src=$1, my $dst=$2;

        my $key="$src-$dst";
        if($streams{$key}) {
            my ($sp, $rs, $rd, $rt, $pha, $f, $s, $t) = @{$streams{$key}};
            $pha++;
            if($pha>3) { print "Note @ line $line: $key out of phase $pha\n"; }
            $streams{$key} = [ $sp, $rs, $rd, $rt, $pha, $f, $s, $t ];

        } else {
            $streams{$key} =
                [
                    0, # Sent packets
                    0, # Received with single redundancy
                    0, # Received with double redundancy
                    0, # Received with triple redundancy
                    1, # Phase in redundancy group
                    0, # Received first
                    0, # Received second
                    0, # Received third
                ];
        }

    } elsif(/\[D\] Node 0: \((\d+),(\d+)\) ---/ ||
            /\[D\] - \((\d+),(\d+)\)/) {
        my $src=$1, my $dst=$2;

        my $key="$src-$dst";
        if($streams{$key}) {
            my ($sp, $rs, $rd, $rt, $pha, $f, $s, $t) = @{$streams{$key}};
            $sp++;
            $rs++ if($f == 1); # Single redundancy would send only the first
            $rd++ if(($f == 1) || ($s == 1)); # Either first or second for double
            $rt++ if(($f == 1) || ($s == 1) || ($t == 1));
            if($pha>3) { print "Note @ line $line: $key out of phase $pha\n"; }
            $streams{$key} = [ $sp, $rs, $rd, $rt, 0, 0, 0, 0 ];

        } else {
            print "Note @ line $line: $key starts out of phase\n";
            $streams{$key} =
                [
                    1, # Sent packets
                    0, # Received with single redundancy
                    0, # Received with double redundancy
                    0, # Received with triple redundancy
                    0, # Phase in redundancy group
                    0, # Received first
                    0, # Received second
                    0, # Received third
                ];
        }

    }
}

print "\n\nStream stats:\n";
foreach(sort keys %streams) {
    my $key=$_;
    my $val=$streams{$_};
    my ($sp, $rs, $rd, $rt, $pha, $f, $s, $t) = @{$val};
    my $rels=100*$rs/$sp;
    my $reld=100*$rd/$sp;
    my $relt=100*$rt/$sp;
    print "[$key]: SENT=$sp RCVD_SINGLE=$rs RCVD_DOUBLE=$rd RCVD_TRIPLE=$rt REL_SINGLE=$rels% REL_DOUBLE=$reld%  REL_TRIPLE=$relt%\n";
}
