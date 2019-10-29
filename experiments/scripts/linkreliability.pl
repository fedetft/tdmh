#!/usr/bin/perl

# use "perl linkreliability.pl < logfile"
# or  "perl linkreliability.pl notimeweight < logfile"

use strict;

if($ARGV[0] ne "notimeweight") {

    ## Time weighted link reliability
    print "Time weighted link reliability\n";
    my %links;
    my %curlinks;
    my $begintime=-1;
    my $curtime=0;
    my $lasttime=0;

    while(<STDIN>)
    {
        # NOTE: at the time of writing this script "Begin Topology" does not print
        # the network time, so we "scavenge" the closest network time. When
        # the logging format is extended "[SC] Begin Topology NT=..." will match
        # twice
        if(/ NT=(\d+)/) {
            $curtime=$1;
        }

        if(/(\[\d+ - \d+\])/) {
            $curlinks{$1}=1; #1 is not important, using hash as set
        } elsif(/\[SC\] Begin Topology/) {
            if($begintime==-1) {
                $begintime=$curtime;
            } else {
                foreach(keys %curlinks) {
                    $links{$_}+=$curtime-$lasttime;
                }
            }
            %curlinks=();
            $lasttime=$curtime;
        }
    }

    die "Wrong file" if($begintime==-1);

    foreach(keys %curlinks) {
        $links{$_}+=$curtime-$lasttime;
    }

    foreach(sort keys %links) {
        printf("$_ %7.2f %%\n",$links{$_}/($curtime-$begintime)*100);
    }

} else {

    # Occurrence counting link reliability
    print "Occurrence counting link reliability\n";
    my %links;
    my $uplinks=0;
    while(<STDIN>)
    {
        if(/(\[\d+ - \d+\])/)
        {
            $links{$1}=$links{$1}+1;
        } elsif(/\[SC\] Begin Topology/) {
            $uplinks++;
        }
    }

    foreach(sort keys %links)
    {
        printf("$_ %7.2f %%\n",$links{$_}/$uplinks*100);
    }

}
