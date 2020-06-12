set yrange [0:60]
set xrange [-0.5:5.5]
set key left top
set grid ytics lt 0 lw 1 lc rgb "#bbbbbb"
set style line 5 lw 3 pt 6
#set multiplot layout 1,2
set style data candlesticks
set boxwidth 0.5
set xlabel "Component configuration"
set ylabel "Latency in ms"
plot "< awk '$4 == 32' latency_100k.txt" using 0:($9 * 1000):($7 * 1000):($15 * 1000):($13 * 1000):xtic(stringcolumn(1).','.stringcolumn(2))