@echo off

set OUTFILE=master_tx.csv
if exist %OUTFILE% del %OUTFILE%

echo Creating %OUTFILE%. This window will close when this is done.
for /L %%G in (1, 1, 1000) do echo "0, 0" >> %OUTFILE%
for /L %%G in (1, 1, 100) do echo "1447, 1447" >> %OUTFILE%
for /L %%G in (1, 1, 1000) do echo "0, 0" >> %OUTFILE%
