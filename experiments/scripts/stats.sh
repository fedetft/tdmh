#!/bin/bash

fail() {
    echo "use: ./stats.sh logfile"
    exit 1
}

if [[ ! -f "$1" ]]; then
    fail
fi

perl -e 'my $l=0;while(<>){$l++;next unless /Scheduler stack (\d+)/; print "$l $1\n";}' < $1 > schedStack.txt
perl -e 'my $l=0;while(<>){$l++;next unless /MAC stack (\d+)/; print "$l $1\n";}'       < $1 > macStack.txt
perl -e 'my $l=0;while(<>){$l++;next unless /max size (\d+)/; print "$l $1\n";}'        < $1 > logBytes.txt
perl -e 'my $l=0;while(<>){$l++;next unless /MinHeap=(\d+)/; print "$l $1\n";}'         < $1 > heapFree.txt

scilab -f "$(dirname "$0")"/stats.sce

rm schedStack.txt macStack.txt logBytes.txt heapFree.txt
