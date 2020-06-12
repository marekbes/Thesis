
set xtics (10000,25000,50000,75000,10000)
set xrange [0:120000]
set yrange [0:24]
set key below
set grid ytics lt 0 lw 1 lc rgb "#bbbbbb"
set style line 5 lw 3 pt 6
set ylabel "Throughput in GB/s"
set style data lp
set title "Window slide impact on throughput"
set bmargin 3.5
set xlabel "Window Slide in elements"
plot "< awk '$1 == \"count\" && $2 == \"eager\"' tumbling_run_0.txt" using 3:4 title "Count,Eager",\
    "< awk '$1 == \"count\" && $2 == \"delayed\"' tumbling_run_0.txt" using 3:4 title "Count,Delayed",\
    "< awk '$1 == \"count\" && $2 == \"direct\"' tumbling_run_0.txt" using 3:4 title "Count,Direct",\
    "< awk '$1 == \"clock\" && $2 == \"eager\"' tumbling_run_0.txt" using 3:4 title "Clock,Eager",\
    "< awk '$1 == \"clock\" && $2 == \"delayed\"' tumbling_run_0.txt" using 3:4 title "Clock,Delayed",\
    "< awk '$1 == \"clock\" && $2 == \"direct\"' tumbling_run_0.txt" using 3:4 title "Clock,Direct"