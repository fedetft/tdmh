#!/usr/bin/perl

use warnings;
use strict;
use List::Util qw[min max];

my $line=0;
my %streams;

while(<>)
{
    $line++;
    if(/\[A\] Received data from \((\d+),(\d+)\): ID=(\d+) Time=(\d+) MinHeap=(\d+) Heap=\d+ Counter=(\d+)/ ||
       /\[A\] R \((\d+),(\d+)\) ID=(\d+) T=(\d+) MH=(\d+) C=(\d+)/) {
        my $src=$1, my $dst=$2, my $id=$3, my $t=$4, my $mheap=$5, my $ctr=$6;

        my $error=0;
        if($src != $id) { print "Error @ line $line: SRC=$src != ID=$id\n"; $error++; } 
        if($dst != 0)   { print "Error @ line $line: DST=$dst != 0\n";      $error++; }
        # Skip execuring follwing code if errors were found
        next if($error!=0);

        my $key="$src-$dst";
        if($streams{$key}) {
            # Stream already seen
            my ($octr, $ot, $omheap, $oreboot, $oclosed, $osentctr, $osentlog, $orecvd, $ofailed) = @{$streams{$key}};
            
            if($t < $ot) {
                print "Note @ line $line: node ID=$id rebooted (T went from $ot to $t)\n";
                $oreboot++;
            }
            
            if($ctr >= $octr) {
                $osentctr += $ctr - $octr;
            if($ctr > $octr+1) {
                print "Note @ line $line: node ID=$id CTR went from $octr to $ctr, missed print\n";
            }
            } else {
                print "Note @ line $line: node ID=$id CTR went from $octr to $ctr, closed stream?\n";
                $oclosed++;
                # We may have lost an unknown number of packets
                $osentctr += $ctr;
            }
            
            $streams{$key} =
                [
                    $ctr,                # Last ctr value
                    $t,                  # Last time value
                    min($mheap,$omheap), # Minimum heap
                    $oreboot,            # Reboot count
                    $oclosed,            # Closed stream count
                    $osentctr,           # Packets sent measured using ctr
                    $osentlog+1,         # Packets sent measured by lines in the log
                    $orecvd+1,           # Packets explicitly received
                    $ofailed             # Packets explicitly not received
                ];

        } else {
            # First time we've seen this stream
            print "Stream [$key] first seen @ line $line\n";
            print "Note @ line $line: stream [$key] starts with CTR=$ctr and not 0\n" if $ctr!=0;
            $streams{$key} =
                [
                    $ctr,   # Last ctr value
                    $t,     # Last time value
                    $mheap, # Minimum heap
                    0,      # Reboot count
                    0,      # Closed stream count
                    $ctr,   # Packets sent measured using ctr
                    1,      # Packets sent measured by lines in the log
                    1,      # Packets explicitly received
                    0       # Packets explicitly not received
                ];
        }

    } elsif(/\[E\] No data received from Stream \((\d+),(\d+)\)/ ||
            /\[E\] M \((\d+),(\d+)\)/) {

        my $src=$1, my $dst=$2;

        print "Error @ line $line: DST=$dst != 0\n"      if $dst != 0;

        my $key="$src-$dst";
        if($streams{$key}) {
            # Stream already seen
            my ($octr, $ot, $omheap, $oreboot, $oclosed, $osentctr, $osentlog, $orecvd, $ofailed) = @{$streams{$key}};
            
            $streams{$key} =
                [
                    $octr,               # Last ctr value
                    $ot,                 # Last time value
                    $omheap,             # Minimum heap
                    $oreboot,            # Reboot count
                    $oclosed,            # Closed stream count
                    $osentctr,           # Packets sent measured using ctr
                    $osentlog+1,         # Packets sent measured by lines in the log
                    $orecvd,             # Packets explicitly received
                    $ofailed+1           # Packets explicitly not received
                ];
            
        } else {
            # First time we've seen this stream
            print "Stream [$key] first seen @ line $line\n";
            $streams{$key} =
                [
                    0,      # Last ctr value
                    0,      # Last time value
                    999999, # Minimum heap
                    0,      # Reboot count
                    0,      # Closed stream count
                    0,      # Packets sent measured using ctr
                    1,      # Packets sent measured by lines in the log
                    0,      # Packets explicitly received
                    1       # Packets explicitly not received
                ];
        }

    }
}

foreach(sort keys %streams) {
    my $key=$_;
    my $val=$streams{$_};
    my ($ctr, $t, $mheap, $reboot, $closed, $sentctr, $sentlog, $recvd, $failed) = @{$val};
    my $rcvdfailed=$recvd+$failed;
    unless($sentctr == $sentlog && $sentlog == $rcvdfailed) {
        print "Note: [$key] mismatch SENTCTR=$sentctr SENTLOG=$sentlog RECVD+FAILED=$rcvdfailed\n";
    }
}

print "\n\nStream stats:\n";
foreach(sort keys %streams) {
    my $key=$_;
    my $val=$streams{$_};
    my ($ctr, $t, $mheap, $reboot, $closed, $sentctr, $sentlog, $recvd, $failed) = @{$val};
    my $sent=max($sentctr,$sentlog);
    my $reliability=100*$recvd/$sent;
    print "[$key]: MIN_HEAP=$mheap REBOOT=$reboot CLOSED=$closed SENT=$sentctr,$sentlog RECVD=$recvd FAILED=$failed RELIABILITY=$reliability%\n";
}
