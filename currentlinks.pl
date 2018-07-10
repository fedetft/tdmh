#!/usr/bin/perl

use strict;

my $links;

while(<>)
{
    if(/\[U\] Current topology/) {
		$links = "";
	}

	if(/(\[\d+ - \d+\])/)
	{
		$links .= "$1\n";
	}
}
print($links);
