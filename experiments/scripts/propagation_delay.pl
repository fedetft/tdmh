#!/usr/bin/perl

use warnings;
use strict;

while(<>)
{
	next unless (/\[U\] Propagation delay: (-?\d+), (-?\d+), (-?\d+)/);
	print("$1 $2 $3\n");
}