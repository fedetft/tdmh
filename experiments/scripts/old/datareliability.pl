#!/usr/bin/perl

use strict;

my %data;     # Hash mapping succeding phases's schedule id to occurrences
my %ids;      # Hash mapping schedule id to occurrences

while(<>)
{
	if(/\[D\]#(\d+) (.)/)
    {
        if ($2 != "X")
        {
            $data{$1}=$data{$1}+1;
        }
        $ids{$1}=$ids{$1}+1
   }
}

foreach(sort keys %ids)
{
	printf("$_\t%7.3f %%\n",$data{$_}/$ids{$_}*100);
}
