#!/usr/bin/env python

# Generate a permutation traffic matrix.
# python gen_pemutation.py <nodes> <conns> <flowsize> <extrastarttime>
# Parameters:
# <nodes>   number of nodes in the topology
# <conns>    number of active connections
# <flowsize>   size of the flows in bytes
# <extrastarttime>   How long in microseconds to space the start times over (start time will be random in between 0 and this time).  Can be a float.
# <randseed>   Seed for random number generator, or set to 0 for random seed

import os
import sys
from random import seed, shuffle
import random
#print(sys.argv)
if len(sys.argv) != 8:
    print("Usage: python gen_pemutation.py <filename> <nodes> <conns> <flowsize> <extrastarttime> <randseed> <num_perm>")
    sys.exit()
filename = sys.argv[1]
nodes = int(sys.argv[2])
conns = int(sys.argv[3])
flowsize = int(sys.argv[4])
extrastarttime = float(sys.argv[5])
randseed = int(sys.argv[6])
num_perm = int(sys.argv[7])

print("Nodes: ", nodes)
print("Connections: ", conns)
print("Flowsize: ", flowsize, "bytes")
print("ExtraStartTime: ", extrastarttime, "us")
print("Random Seed ", randseed)
print("Num Perm ", randseed)

f = open(filename, "w")
print("Nodes", nodes, file=f)
print("Connections", conns, file=f)

if randseed != 0:
    seed(randseed)

idx = 0
for permutation_n in range(0,num_perm):
    available_nodes = list(range(0,nodes))
    extracted = 0
    for node in range(0,nodes):
        idx += 1
        if node < nodes / 2:
            print(node)
            print(available_nodes)
            extracted = random.randint(nodes/2, nodes-1)
            while (extracted not in available_nodes):
                extracted = random.randint(nodes/2, nodes-1)
                #print(extracted)
            available_nodes.remove(extracted)
        else:
            extracted = random.randint(0, nodes/2-1)
            while (extracted not in available_nodes):
                extracted = random.randint(0, nodes/2-1)
            available_nodes.remove(extracted)

        out = str(node) + "->" + str(extracted) + " id " + str(idx) + " start " + str(int(extrastarttime * 1000000)) + " size " + str(flowsize)
        print(out, file=f)
        print(out)


f.close()
