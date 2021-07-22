#!/usr/bin/perl

die "use: perl rssi.pl maxNodes < logfile" unless($#ARGV+1==1);

my $maxNodes = $ARGV[0];
my $nodeId = $ARGV[1]; # nodeId = node that logged the log file
my $prec_id = $maxNodes;
my $found = 0;

# create csv header
for(my $i = $prec_id - 1; $i >= 0; $i--) {
    if ($i != 0) {
	    print($i, ",");
    }
    else {
        print($i, "\n");
    }
}

my $node_num = 0;

while(<STDIN>)
{
    if (/\[U\]<-N=(\d+) @(\d+) -(\d+)dBm/) {
        # some nodes not connected exist
        # (to the network or to the node where the log comes from)
        if ($1 < $prec_id - 1) {
            for(my $i = $prec_id; $i > $1+1; $i--) {
                print("nan,");
            }
        }
        
        print("-", $3);
        $found = 1;
        $prec_id = $1;

        $node_num++;
    }
    else {
        next;
    }
    
    # found all the nodes
    if ($node_num == $maxNodes and $found) {
    #if ($prec_id == 0 ) { 
        print("\n");
        $prec_id = $maxNodes;
        $node_num = 0;
    }
    else {
        print(",");
    }

    $found = 0;
}

# complete line if needed, to create a valid csv
if ($prec_id != $maxNodes) {
    for(my $i = $prec_id-1; $i >= 0; $i--) {
        if ($i != 0) {
            print("nan,");
        }
        else {
            print("nan\n");
        }
    }
}