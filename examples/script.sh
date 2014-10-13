#!/bin/bash
i=0
while read host; do
  ip_node=$(grep $host /etc/hosts | cut -d'	' -f1)
  
  case $i in 
  0)
  jmip=$(grep $host /etc/hosts | cut -d'	' -f1)
  echo ssh $host "LD_LIBRARY_PATH=\$HOME/codes/spitz/examples SPITS_LOG_LEVEL=2 \$HOME/codes/spitz/examples/spitz 0 $jmip \$HOME/codes/spitz/examples/prime.so 30" &
  ssh $host "LD_LIBRARY_PATH=\$HOME/codes/spitz/examples SPITS_LOG_LEVEL=2 \$HOME/codes/spitz/examples/spitz 0 $jmip \$HOME/codes/spitz/examples/prime.so 30" &
  ;;
  1)
  echo ssh $host "LD_LIBRARY_PATH=\$HOME/codes/spitz/examples SPITS_LOG_LEVEL=2 \$HOME/codes/spitz/examples/spitz 1 $jmip \$HOME/codes/spitz/examples/prime.so 30" &
  ssh $host "LD_LIBRARY_PATH=\$HOME/codes/spitz/examples SPITS_LOG_LEVEL=2 \$HOME/codes/spitz/examples/spitz 1 $jmip \$HOME/codes/spitz/examples/prime.so 30" &
  ;;
  *)
  echo ssh $host "LD_LIBRARY_PATH=\$HOME/codes/spitz/examples SPITS_LOG_LEVEL=2 \$HOME/codes/spitz/examples/spitz 2 $jmip \$HOME/codes/spitz/examples/prime.so 30" &
  ssh $host "LD_LIBRARY_PATH=\$HOME/codes/spitz/examples SPITS_LOG_LEVEL=2 \$HOME/codes/spitz/examples/spitz 2 $jmip \$HOME/codes/spitz/examples/prime.so 30" &
  ;;
  esac

  echo NODE$i : $host has ip : $ip_node
  i=`expr $i + 1`
done < $PBS_NODEFILE
wait
