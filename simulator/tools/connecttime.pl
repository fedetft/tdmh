#!/usr/bin/perl

# Compute topology collection convergence time after a node disconnected given ned and postprocessed elog

use warnings;
use strict;

my $verbose=0; # Prints more info

my ($ned_filename, $elog_filename)=@ARGV;

open(my $ned,  '<', $ned_filename)  or die "can't open ned file";
open(my $elog, '<', $elog_filename) or die "can't open elog file";

# Read links from the ned file

my @links;
while(<$ned>)
{
	next unless(/n(\d+)\.wireless\+\+ <--> n(\d+)\.wireless\+\+;/);
	if($1<$2) { push(@links,"$1 $2"); }
	else      { push(@links,"$2 $1"); }
}
die "wrong ned file format?" unless(@links);
@links=sort(@links);

sub compare {
	my ($aa, $bb)=@_;
	my @a=@{ $aa };
	my @b=@{ $bb };
	return 0 if($#a != $#b);
	for(my $i=0;$i<$#a;$i++) { return 0 if($a[$i] ne $b[$i]); }
	return 1;
}

# Compute the topology colletion convergence time from the omnet++ log file

my $flag=0;
my $start;
my $end=-1;
while(<$elog>)
{
	if(/n0:\[U\] Current topology @(\d+):/) {
		my $t=$1/1e9;
		my @topology;
		while(<$elog>) {
			last unless(/n0:\[(\d+) - (\d+)\]/);
			if($1<$2) { push(@topology,"$1 $2"); }
			else      { push(@topology,"$2 $1"); }
		}
		@topology=sort(@topology);
		# Now we have the topology and time when the master computed it
		
		if($flag==1 && compare(\@links,\@topology)) {
			$end=$t;
			print "end=$end\n" if($verbose);
			$flag=2;
		}
		if($flag==2 && !compare(\@links,\@topology)) {
			$end=-1;
			print "wrong topology after convergence @ $t\n" if($verbose);
		}
	} elsif(/===> Starting @ (\d+)/) {
		die "Multiple '===> Starting' marks" unless($flag==0);
		$flag=1;
		$start=$1/1e9;
		print "start=$start\n" if($verbose);
	}
}

die "wrong elog file format?" if($flag==0);

print "convergence time=" if($verbose);
my $delta=$end-$start;
if($end==-1) { print "does not converge\n"; } else { print "$delta\n"; }
