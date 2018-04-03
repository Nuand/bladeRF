#!/usr/bin/env python3
#
# Convert a given bladeRF CSV samples file to SC16Q11 binary format.
#

import csv
import struct
import sys
from   pathlib import Path

def csv2bin( csvfile = None, binfile = None ):
    with open(csvfile, 'r') as c:
        with open(binfile, 'wb') as b:
            csvreader = csv.reader(c, delimiter=',')
            count = 0
            for row in csvreader:
                for col in row:
                    # Little endian 16-bit integers (shorts)
                    b.write( struct.pack("<h", int(col.strip())) )
                count += 1
            print( "Processed", str(count), "samples." )

if( len(sys.argv) != 3 ):
    print( "Usage:", sys.argv[0], "in.csv out.bin" )
    sys.exit(-1)
else:
    csvfile = sys.argv[1]
    binfile = sys.argv[2]
    if( Path(csvfile).is_file() ):
        csv2bin( csvfile, binfile )
    else:
        print( "File does not exist:", csvfile )
        sys.exit(-1)
