#!/bin/sh
make
./nedgen_hexagon RHex128 128 128 10 1000 1 >topology
# scilab -f plot.sci
