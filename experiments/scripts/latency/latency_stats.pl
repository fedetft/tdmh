#!/usr/bin/perl

#
# OUTPUT FILE FOR PLOT FORMAT:
# numSamples latency average stddev
#

die "use: perl latency_stats.pl --stats < logfile \n     perl latency_stats.pl --plot streamsrc streamdst < logfile\n" unless($#ARGV+1==1 || $#ARGV+1==3);

my %streams;

my $numArgs=$#ARGV+1;

my $computeStats = 0;      # output to screen stats for all streams
my $outputDataForPlot = 0; # output to file data for plotting single stream

# src and dst nodes id
my $src = 0;
my $dst = 0;

sub printStreamHeader() {
    print "\n-------------------\n";
    print "   Stream: ($src,$dst)\n";
    print "-------------------\n";
}

sub parseLog() {
    local $data = 0;

    while(<STDIN>) {
        
        if ($outputDataForPlot) { # specific src and dst passed as script arguments
            next unless /\[A\] \($src,$dst\): L=(\d+)/;
            $data = $1; #/ 1000000.0; #nanoseconds to milliseconds
        }
        elsif ($computeStats) {
            next unless /\[A\] \((\d+),(\d+)\): L=(\d+)/;
            $src = $1;
            $dst = $2;
            $data = $3;
        }

        my $key="$src-$dst";

        if($streams{$key}) {
            local ($min, $max, $sum, $avg, $m2, $stddev, $numSamples) = @{$streams{$key}};

            $numSamples += 1;

            $totalSum += $data;

            # update avg and variance
            local $delta = $data - $avg;
            $avg += $delta / $numSamples;
            $m2 += $delta * ($data - $avg);
            $stddev = sqrt($m2 / ($numSamples - 1));

            # update max and min
            if($data > $max) {
                $max = $data;
            }
            elsif ($data < $min) {
                $min = $data;
            }

            if ($outputDataForPlot) {
                print "$numSamples $data $avg $stddev\n"; # output latency in nanoseconds
            }

            $streams{$key} = [ $min, $max, $sum, $avg, $m2, $stddev, $numSamples ];

        } else {
            $streams{$key} =
                [
                    $data, # Minimum
                    $data, # Maximum
                    $data, # Sum
                    $data, # Average
                    0.0,   # m2
                    0.0,   # Standard deviation
                    1,     # Number of samples
                ];
            
            
            if ($outputDataForPlot) {
                print "1 $data $data 0\n"; # output latency in nanoseconds
            }
        }
    }

    if ($computeStats) { # print statistics for each stream if needed
        #print "\nSTREAMS LATENCY STATS:\n";
        local $samplesTot=0, local $maxMinDeltaTot=0, local $stddevTot=0;
        print "\nStream latency stats (in ms):\n";
        foreach(sort keys %streams) {
            local $key=$_;
            local $val=$streams{$_};
            local ($min, $max, $sum, $avg, $m2, $stddev, $numSamples) = @{$val};
            $min = sprintf("%.6f", $min / 1000000);
            $max = sprintf("%.6f", $max / 1000000);
            local $maxMinDelta = sprintf("%.6f", $max - $min);
            $avg = sprintf("%.6f", $avg / 1000000);
            #$m2 = sprintf("%.6f", $m2 / 1000000);
            $stddev = sprintf("%.6f", $stddev / 1000000);
            
            # for overall results
            $samplesTot     += $numSamples;
            $maxMinDeltaTot += $maxMinDelta;
            $stddevTot      += $stddev;

            print "[$key]: N=$numSamples MIN=$min MAX=$max MAX/MIN_DELTA=$maxMinDelta AVG=$avg STDDEV=$stddev\n";
        }
        local $numStreams = keys %streams;
        $maxMinDeltaTot   = sprintf("%.6f", $maxMinDeltaTot / $numStreams);
        $stddevTot        = sprintf("%.6f", $stddevTot / $numStreams);
        # non-sense to compute average of min, max, and avg values, they are specific for 
        # each stream and depend on schedule (i.e. latencies are very different in each stream)
        print "TOTAL: N=$samplesTot MAX/MIN_DELTA=$maxMinDeltaTot STDDEV=$stddevTot\n\n";
    }
}

if ($numArgs == 1) {
    if (@ARGV[0] == "--stats") {
        $computeStats = 1;
        #print "Compute statistics for all streams (computeStats=$computeStats)\n";
    }
    else {
        local $p = @ARGV[0];
        print "Invalid parameter $p, allowed is --stats\n";
        exit;
    }
}
elsif ($numArgs == 3) {
    if (@ARGV[0] == "--plot") {
        $outputDataForPlot = 1;
        $src = @ARGV[1];
        $dst = @ARGV[2];
        #print "Output data for stream ($src,$dst)\n";
    }
    else {
        local $p = @ARGV[0];
        print "Invalid parameter $p, allowed is --plot\n";
        exit;
    }
}

parseLog();

# if ($numArgs == 2) {
#     parseLog();
# }
# elsif ($numArgs == 3) {
#     if ($ARGV[2] eq "--stats") {
#         latencyStats();
#     }
#     else {
#         my $invalidArg = $ARGV[2];
#         printStreamHeader();
#         print "Invalid parameter $invalidArg, can't compute latency statistics\n";
#     }
# }