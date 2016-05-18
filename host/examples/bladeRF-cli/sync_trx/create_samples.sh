#!/bin/sh
#
# Creates a master_tx.csv file
################################################################################

OUTFILE=master_tx.csv

rm -f ${OUTFILE}

# Write zeros
i=0
while [ $i -lt 1000 ]; do
	echo "0, 0" >> ${OUTFILE}
	i=$(expr $i + 1)
done

# Write a pulse (i = q = sqrt(2)/2)
i=0
while [ $i -lt 100 ]; do
	echo "1447, 1447" >> ${OUTFILE}
	i=$(expr $i + 1)
done

# Write zeros
i=0
while [ $i -lt 1000 ]; do
	echo "0, 0" >> ${OUTFILE}
	i=$(expr $i + 1)
done
