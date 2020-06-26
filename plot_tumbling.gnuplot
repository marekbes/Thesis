set yrange [0:25]
set xrange [0:100000]
set xtics (10000,25000,50000,75000,100000)
set grid ytics lt 0 lw 1 lc rgb "#bbbbbb"
set title "Window slide impact on throughput"
set ylabel "Throughput in GB/s"
set style data lp
set xlabel "Window Slide"
set key below
plot "< awk '$1 == \"Count\" && $2 == \"Direct\"' results_sliding.txt" using 3:4 ls 1 pt 8 title "Count,Direct",\
    "< awk '$1 == \"Clock\" && $2 == \"Direct\"' results_sliding.txt" using 3:4 ls 2 pt 2 title "Clock,Direct",\
    "< awk '$1 == \"Count\" && $2 == \"Eager\"' results_sliding.txt" using 3:4 ls 3 pt 3 title "Count,Eager",\
    "< awk '$1 == \"Clock\" && $2 == \"Eager\"' results_sliding.txt" using 3:4 ls 4 pt 4 title "Clock,Eager",\
    "< awk '$1 == \"Count\" && $2 == \"Delayed\"' results_sliding.txt" using 3:4 ls 7 pt 1 title "Count,Delayed",\
    "< awk '$1 == \"Clock\" && $2 == \"Delayed\"' results_sliding.txt" using 3:4 ls 6 pt 6 title "Clock,Delayed"