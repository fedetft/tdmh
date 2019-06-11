#!/usr/bin/env python3

# Author: Federico Amedeo Izzo, federico.izzo42@gmail.com

import os
import sys
import re
from collections import defaultdict

if len(sys.argv) < 2:
    print("Usage: {} master_log_path".format(sys.argv[0]))
    quit()

file_path = sys.argv[1]
regex = 'ID=(?P<id>\d+).+Counter=(?P<counter>\d+)'
results = defaultdict(list)
totpackets = dict()
packetloss = dict()
errors = dict()
reliability = dict()

print("Analyzing log file:{}".format(file_path))

# Extract ID and counter from log lines
if os.path.exists(file_path):
    with open(file_path, "r") as file:
        for line in file:
            match = re.search(regex, line)
            if match:
                results[match.group('id')].append(match.group('counter'))
else:
    print("File not found!")


# Check counter for errors
print("\n### Errors ###")
for ID in results.keys():
    errors[ID] = 0
    packetloss[ID] = 0
    oldvalue = results.get(ID)[0]
    for counter in results.get(ID)[1:]:
        if int(counter) - int(oldvalue) != 1:
            print("ID={}: {} {} are not sequential".format(ID,oldvalue,counter))
            errors[ID] += 1
            if int(counter) > int(oldvalue):
                packetloss[ID] += int(counter) - int(oldvalue) - 1
        oldvalue = counter

# Calculate reliability percentage
# Every time we read a line containing ID=n Counter=m a packet is received
# If we count the lines, we get the number of packets received
# To get the total number of packets, we must add the calculated number of lost packets
for ID in errors.keys():
    totpackets[ID] = len(results.get(ID)) + packetloss[ID]
    reliability[ID] = (totpackets[ID] - packetloss[ID]) / totpackets[ID]

# Print results
print("\n### Results ###")
for ID in errors.keys():
  print("ID={}: packets={}, lost_packets={},  errors={}, reliability={}".format(ID,totpackets[ID],packetloss[ID],errors[ID],reliability[ID]))
