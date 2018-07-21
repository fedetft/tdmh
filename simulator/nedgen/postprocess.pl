#!/usr/bin/perl

# Postprocess elog keeping only print_dbg messages

use warnings;
use strict;
use File::Temp qw/ tempfile tempdir /;

foreach my $filename (@ARGV)
{
	open(my $in,  '<', $filename)  or die "can't open $filename";
	my ($out, $tempfilename) = tempfile();
	while(<$in>)
	{
		next if(/🔴|🔵|⚪/);
		next unless(/^- INFO (Root)?Node (.*)$/);
		print $out "$2\n"
	}
	close($in);
	close($out);
	`mv $tempfilename $filename`
}
