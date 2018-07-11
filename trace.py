#!/bin/env python
import sys
import socket

try:
    srcname = sys.argv[2]
    destname = sys.argv[3]
except:
    srcname = socket.gethostname().split('.')[0]
    destname = sys.argv[2]

print 'tracing route from', srcname,'to',destname

with open(sys.argv[1]) as fd: 
    for line in fd:
        data = line.split()

        if "SW" in data:
            swname = data[2]
        if srcname in data:
            sswname = swname
            iport = data[2]
            eport = data[1]
        if destname in data:
            dlid = data[0]

    print srcname, '[' + iport + ']', '->', '[' + eport + ']', sswname, 
    trace = True
    while trace:
        fd.seek(0)
        find_dlid = False
        for line in fd:
            data = line.split()
            if "SW" in data:                
                if sswname == ' '.join(data[2:]): 
                    find_dlid = True
            if find_dlid:
                if dlid == data[0]:
                    sswname = ' '.join(data[3:])
                    iport = data[2]
                    eport = data[1]
                    print '[' + eport + ']', '->', '[' + iport + ']', sswname,
                    find_dlid = False
                if destname in data:
                    trace = False
                    print
                    
