cat experiment2_out.txt | awk '{if ($1=="Experiment:"){speed = $3; if (NF==13) {oversub=$5 ; size = $6;conns = $8; delta = $10; scheme = $13;} else {oversub=1 ; size = $5;conns = $7; delta = $9; scheme = $12;}} if ($2=="Tail"){if (scheme=="rccc") printf ("%s%s %s %s%s%s%s%s %s %s %s ",speed,"Gbps",oversub,size,"MB ",conns," connections ",delta,"us",scheme,$4); else if (scheme=="nscc") print scheme,$4; }}' | awk '{print $0, $11/$9;sum+=$11/$9;cnt++;}' | tr "Gb" " G" | tr "MB" " M" | awk '{print $0,($4*$6*8000/$1+8);}' > incast.out

echo "Generating incast.png"
gnuplot incast.plot




