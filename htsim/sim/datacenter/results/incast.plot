set term png

set xlabel "Experiment number"
set ylabel "FCT inflation over optimal"

set xrange [0:]
#set xtics 5

set output "incast.png"

plot "incast.out" using 0:($11/$15) t "RCCC", "" using 0:($13/$15) t "NSCC"