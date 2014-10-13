#!/bin/bash
i=0
while read host; do
  case $i in 
  0)
  jmip=$(grep $host /etc/hosts | cut -d'	' -f1)
  ssh $host "LD_LIBRARY_PATH=\$HOME/codes/spitz/examples SPITS_LOG_LEVEL=2 \$HOME/codes/spitz/examples/spitz 0 $jmip prime.so 30" &
  ;;
  1)
  ssh $host "LD_LIBRARY_PATH=\$HOME/codes/spitz/examples SPITS_LOG_LEVEL=2 \$HOME/codes/spitz/examples/spitz 1 $jmip prime.so 30" &
  ;;
  *)
  ssh $host "LD_LIBRARY_PATH=\$HOME/codes/spitz/examples SPITS_LOG_LEVEL=2 \$HOME/codes/spitz/examples/spitz 2 $jmip prime.so 30" &
  ;;
  esac
  i=`expr $i + 1`
done < $PBS_NODEFILE
wait
