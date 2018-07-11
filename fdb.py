#!/bin/env python
import sys
import socket
import cPickle as p
import time

s=time.time()
try:
    with open("/tmp/lids.p") as fd:
        lids = p.load(fd)
    with open("/tmp/switch.p") as fd:
        switch = p.load(fd)
    print "loading existing lids and switch file"
    print time.time()-s
except:
    with open("lids.txt") as fd:
        lids = {}
        for line in fd:
            data = line.split()
            try:
                lids[data[0]] = (data[2], ' '.join(data[4:]))
            except: pass
    with open("/tmp/lids.p", "w") as fd:
        p.dump(lids, fd, p.HIGHEST_PROTOCOL)

    with open("linear_fdb.txt") as fd:
        switch = {}
        for line in fd:
            data = line.split()
            if "SW" in data:
                swname = ' '.join(data[2:])
                switch[swname] = {}
                continue
            if switch and swname in switch:
                try:
                    switch[swname][lids[data[0]]] = (data[1], data[2], ' '.join(data[3:]))
                except: pass
    with open("/tmp/switch.p", "w") as fd:
        p.dump(switch, fd, p.HIGHEST_PROTOCOL)


def trace(srcname, dstname):
    for swname, fdb in switch.iteritems():
        if srcname == fdb[srcname][1:]: break
    print fdb[srcname][2], '[' + fdb[srcname][1] + ']', '->', '[' + fdb[srcname][0] + ']', swname,

    while True:
        row = switch[swname][dstname]
        print '[' + row[0] + ']', '->', '[' + row[1] + ']', row[2], 
        swname = switch[swname][dstname][2]
        if dstname[1] == swname: 
            print
            break


try:
    srcname = sys.argv[1]
    dstname = sys.argv[2]
except:
    srcname = socket.gethostname().split('.')[0]
    dstname = sys.argv[1]

print 'tracing route from', srcname,'to', dstname

srcname = ('1', srcname + ' hfi1_0')
dstname = ('1', dstname + ' hfi1_0')

s=time.time()
trace(srcname, dstname)
trace(dstname, srcname)
print time.time()-s
