#!/usr/bin/perl

use strict;

my %data;     # Hash mapping succeding phases's schedule id to occurrences
my %ids;      # Hash mapping schedule id to occurrences
my %redundancy;

while(<>)
{
	if(/\[D\]#(\d+) (.)/)
    {
        my $fid = int($1 / 10);
        if ($1 % 10 == 0)
        {
            if ($2 != "X")
            {
                $redundancy{$fid} = 1;
            } else {
                $redundancy{$fid} = 0;
            }
        } else {
            if ($2 != "X" || $redundancy{$fid} == 1)
            {
                $data{$fid}=$data{$fid}+1;
            }
            $ids{$fid}=$ids{$fid}+1
        }
   }
}

foreach(sort keys %ids)
{
	printf("$_\t%6.2f%%\n",$data{$_}/$ids{$_}*100);
}
