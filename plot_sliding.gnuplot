set yrange [0:25]
set grid ytics lt 0 lw 1 lc rgb "#bbbbbb"
set ylabel "Throughput in GB/s"
set style data lp
set xlabel "Window Slide"
set key below
plot "< awk '$1 == \"Count\" && $2 == \"Direct\"' sliding_run_0.txt" using 3:4 ls 1 pt 8 title "Count,Direct",\
    "< awk '$1 == \"Clock\" && $2 == \"Direct\"' sliding_run_0.txt" using 3:4 ls 2 pt 2 title "Clock,Direct",\
    "< awk '$1 == \"Count\" && $2 == \"Eager\"' sliding_run_0.txt" using 3:4 ls 3 pt 3 title "Count,Eager",\
    "< awk '$1 == \"Clock\" && $2 == \"Eager\"'  sliding_run_0.txt" using 3:4 ls 4 pt 4 title "Clock,Eager",\
    "< awk '$1 == \"Count\" && $2 == \"Delayed\"' sliding_run_0.txt" using 3:4 ls 7 pt 1 title "Count,Delayed",\
    "< awk '$1 == \"Clock\" && $2 == \"Delayed\"' sliding_run_0.txt" using 3:4 ls 6 pt 6 title "Clock,Delayed"