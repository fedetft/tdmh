#!/usr/bin/python

import re
import sys
from graphviz import Graph

dot = Graph(comment='Topology')
nodes = []

with open(sys.argv[1], 'r') as fin:
    line = fin.readline()
    while line:
        node_a = int(line[1])
        node_b = int(line[5])
        if node_a not in nodes:
             dot.node(str(node_a), str(node_a))
        if node_b not in nodes:
             dot.node(str(node_b), str(node_b))
        m = re.search(r'([^ ]+) %$', line)
        dot.edge(str(node_a), str(node_b), m.group(1) + "%")
        line = fin.readline()
dot.render((sys.argv[1][0: sys.argv[1].rindex(".")] if sys.argv[1].index(".") > 0 else sys.argv[1]) +'.pdf')
