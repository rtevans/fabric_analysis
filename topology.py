#!/bin/env python
import sys

switches = {}
switch_name = None
with open(sys.argv[1]) as fd:
    for line in fd:
        data = line.split()
        if len(data) < 3: continue
        if data[1] == "SW":
            switch_name = ' '.join(data[2:])
            switches[switch_name] = {}
            continue
        if switch_name and "0x" in data[0]:
            switches[switch_name][data[1]] = data[2] + ' ' + ' '.join(data[3:])

for s, pmap in switches.iteritems():
    pmap_idx = sorted(map(int, pmap.keys()))
    for p in pmap_idx:
        print s,p, ' -> ', pmap[str(p)]
