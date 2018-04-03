#!/usr/bin/env python3
#
# Convert a given bladeRF SC16Q11 binary samples file to CSV.
#

import csv
import struct
import sys
from   pathlib import Path

def chunked_read( fobj, chunk_bytes = 4*1024 ):
    while True:
        data = fobj.read(chunk_bytes)
        if( not data ):
            break
        else:
            yield data

def bin2csv( binfile = None, csvfile = None, chunk_bytes = 4*1024 ):
    with open(binfile, 'rb') as b:
        with open(csvfile, 'w') as c:
            csvwriter = csv.writer(c, delimiter=',')
            count = 0
            for data in chunked_read(b, chunk_bytes = chunk_bytes):
                count += len(data)
                for i in range(0, len(data), 4):
                    sig_i, = struct.unpack('<h', data[i:i+2])
                    sig_q, = struct.unpack('<h', data[i+2:i+4])
                    csvwriter.writerow( [sig_i, sig_q] )
    print( "Processed", str(count//2//2), "samples." )

if( len(sys.argv) != 3 ):
    print( "Usage:", sys.argv[0], "in.bin out.csv" )
    sys.exit(-1)
else:
    binfile = sys.argv[1]
    csvfile = sys.argv[2]
    if( Path(binfile).is_file() ):
        bin2csv( binfile, csvfile )
    else:
        print( "File does not exist:", binfile )
        sys.exit(-1)
