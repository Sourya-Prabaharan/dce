#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
awk -F, '
NR>1 { k=$1 SUBSEP $2; count[k]++; total[k]+=$7; util[k]+=$12; if(count[k]==1||$7<low[k])low[k]=$7; if($7>high[k])high[k]=$7 }
END {
 print "| Workload | Workers | Mean wall time (s) | Min–max (s) | Jobs/s | Speedup vs 1 worker | Slot utilization |";
 print "|---|---:|---:|---:|---:|---:|---:|";
 split("primes csv", types, " ");
 for(t=1;t<=2;t++){type=types[t];base=total[type SUBSEP 1]/count[type SUBSEP 1];for(n=1;n<=3;n++){k=type SUBSEP n;m=total[k]/count[k];printf "| %s | %d | %.4f | %.4f–%.4f | %.2f | %.2fx | %.1f%% |\n",type,n,m,low[k],high[k],1/m,base/m,100*util[k]/count[k]}}
}' results/benchmark.csv >results/summary.md
printf '\n| Failure mode | Trial | Observed reassignment (ms) | Correct recovery | Within 5 s |\n|---|---:|---:|---|---|\n' >>results/summary.md
awk -F, 'FNR>1 {printf "| %s | %d | %.3f | %s | %s |\n",$2,$1,$3,($4==1?"yes":"no"),($5==1?"yes":"no")}' results/failure-kill.csv results/failure-heartbeat.csv >>results/summary.md
cat results/summary.md
