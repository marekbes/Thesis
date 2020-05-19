
set xtics (1,2,4,8,16,24,32)
set xrange [0:33]
set yrange [0:24]
set key left top
set grid ytics lt 0 lw 1 lc rgb "#bbbbbb"
set style line 5 lw 3 pt 6
set ylabel "Throughput in GB/s"
set multiplot layout 1,2
set style data lp
set title "10k elements per window"
set bmargin 3.5
set xlabel "Number of workers (threads)"  offset screen 0.25
plot "< awk '$1 == \"count\" && $2 == \"eager\"' run_all_configuration_10k.txt" using 4:6 title "Count,Eager",\
    "< awk '$1 == \"count\" && $2 == \"delayed\"' run_all_configuration_10k.txt" using 4:6 title "Count,Delayed",\
    "< awk '$1 == \"clock\" && $2 == \"eager\"' run_all_configuration_10k.txt" using 4:6 title "Clock,Eager",\
    "< awk '$1 == \"clock\" && $2 == \"delayed\"' run_all_configuration_10k.txt" using 4:6 title "Clock,Delayed"
unset ylabel
set title "100k elements per window"
plot "< awk '$1 == \"count\" && $2 == \"eager\"' latency_100k.txt" using 4:5 title "Count,Eager",\
    "< awk '$1 == \"count\" && $2 == \"delayed\"' latency_100k.txt" using 4:5 title "Count,Delayed",\
    "< awk '$1 == \"count\" && $2 == \"direct\"' latency_100k.txt" using 4:5 title "Count,Direct",\
    "< awk '$1 == \"clock\" && $2 == \"eager\"' latency_100k.txt" using 4:5 title "Clock,Eager",\
    "< awk '$1 == \"clock\" && $2 == \"delayed\"' latency_100k.txt" using 4:5 title "Clock,Delayed",\
    "< awk '$1 == \"clock\" && $2 == \"direct\"' latency_100k.txt" using 4:5 title "Clock,Direct"