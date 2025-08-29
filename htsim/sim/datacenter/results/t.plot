set yrange [1:]
set grid

set xlabel "Flow size (MB)"
set ylabel "Permutation slowdown vs. optimal"

optimal(x)=x*oversub*8000/speed+10

set logscale x 2

plot "32.out" using 1:($4/optimal($1)) w lp lw 3 t "RCCC 32",\
     "32.out" using 1:($11/optimal($1)) w lp t "NSCC 32",\
     "64.out" using 1:($4/optimal($1)) w lp lw 3 t "RCCC 64",\
     "64.out" using 1:($11/optimal($1)) w lp t "NSCC 64",\
     "128.out" using 1:($4/optimal($1)) w lp t "RCCC 128" lw 3,\
     "128.out" using 1:($11/optimal($1)) w lp t "NSCC 128"
