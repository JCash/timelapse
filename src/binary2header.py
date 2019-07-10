#! /usr/bin/env python

import sys, os

def Usage():
    print "Usage: ./binary2header.py <file>"

if __name__ == '__main__':
    if len(sys.argv) < 2:
        Usage()
        sys.exit(1)

    path = sys.argv[1]

    if not os.path.exists(path):
        print "File does not exist:", path
        sys.exit(1)

    if not os.path.isfile(path):
        print "Path is not a file:", path
        sys.exit(1)

    with open(path, 'rb') as f:
        data = f.read()

    name = os.path.split(path)[-1].replace('-', '_').replace('.', '_').upper()

    print "// Generated from: " + os.path.basename(path)
    print "#pragma once"
    print "const size_t %s_LEN = %d;" % (name, len(data))
    print "const char %s_DATA[%d] = {" % (name, len(data))

    step = 16
    count = 0
    for i in xrange(0, len(data), step):
        chunk = data[i:i+step]
        bchunk = ["0x%02X" % ord(c) for c in chunk]
        print "    ", ','.join(bchunk) + ("" if len(chunk) < step else ",")
        count += step

    print "};"
    print ""
