#!/usr/bin/perl

use strict;

my %links;     # Hash mapping links to number of times they appear
my $uplinks=0; # Counts the number of uplink rounds

while(<>)
{
	if(/(\[\d+ - \d+\])/)
	{
		$links{$1}=$links{$1}+1;
	} elsif(/\[U\] Current topology/) {
		$uplinks++;
	}
}

foreach(sort keys %links)
{
	printf("$_ %7.3f %%\n",$links{$_}/$uplinks*100);
}
