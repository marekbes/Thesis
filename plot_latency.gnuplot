set yrange [0:60]
set xrange [-0.5:5.5]
set key title "Window Size"
set key left top
set grid ytics lt 0 lw 1 lc rgb "#bbbbbb"
set bars 10
set style data candlesticks
set boxwidth 0.5
set ylabel "Latency in ms"
set xtics font ", 10"
set style arrow 1 head filled size screen 0.025,30,45 ls 1
set arrow from 4,45 rto 0, 10 as 1
set label '\~800:' at 4,40 center
set arrow from 5,45 rto 0, 10 as 1
set label '\~1000:' at 5,40 center

plot "< sort -k5,5n -k2,2 results_configurations.txt | awk '$4 == 32 && $5 == 10000'" using 0:($10 * 1000):($8 * 1000):($16 * 1000):($12 * 1000):xtic(stringcolumn(1).','.stringcolumn(2)) title '10k' whiskerbars 0.5,\
"< sort -k5,5n -k2,2 results_configurations.txt | awk '$4 == 32 && $5 == 50000'" using 0:($10 * 1000):($8 * 1000):($16 * 1000):($12 * 1000):xtic(stringcolumn(1).','.stringcolumn(2)) title '50k' whiskerbars 0.5