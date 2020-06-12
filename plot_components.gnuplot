set ytics (5,10,15,20)
set xtics (4,8,16,24,32)
set xrange [-2:34]
set yrange [0:25]
set grid ytics lt 0 lw 1 lc rgb "#bbbbbb"
set lmargin at screen 0.15
set rmargin at screen 0.95

LEFT=0.98
DX = 0.29
set multiplot
set offset 0,0,graph 0.05, graph 0.05
set rmargin at screen LEFT-2*DX
set lmargin at screen LEFT-3*DX

set style data lp


set title "Elements per window: 10k " offset screen -0.05
set bmargin 5
set xlabel "Number of workers (threads)" offset screen 0.25,0.01
set ylabel "Throughput in GB/s"
set key off
SIZE = 10000
plot "< awk '$1 == \"Count\" && $2 == \"Direct\" && $5 == ".SIZE."' configuration_run_0.txt" using 4:6 ls 1 pt 8,\
    "< awk '$1 == \"Clock\" && $2 == \"Direct\" && $5 == ".SIZE."' configuration_run_0.txt" using 4:6 ls 2 pt 2,\
    "< awk '$1 == \"Count\" && $2 == \"Eager\" && $5 == ".SIZE."' configuration_run_0.txt" using 4:6 ls 3 pt 3,\
    "< awk '$1 == \"Clock\" && $2 == \"Eager\" && $5 == ".SIZE."' configuration_run_0.txt" using 4:6 ls 4 pt 4,\
    "< awk '$1 == \"Count\" && $2 == \"Delayed\" && $5 == ".SIZE."' configuration_run_0.txt" using 4:6 ls 7 pt 1,\
    "< awk '$1 == \"Clock\" && $2 == \"Delayed\" && $5 == ".SIZE."' configuration_run_0.txt" using 4:6 ls 6 pt 6
unset ylabel
unset xlabel
unset title
set format y ""
set rmargin at screen LEFT-1*DX
set lmargin at screen LEFT-2*DX
set title "50k"
SIZE = 50000
plot "< awk '$1 == \"Count\" && $2 == \"Direct\" && $5 == ".SIZE."' configuration_run_0.txt" using 4:6 ls 1 pt 8,\
    "< awk '$1 == \"Clock\" && $2 == \"Direct\" && $5 == ".SIZE."' configuration_run_0.txt" using 4:6 ls 2 pt 2,\
    "< awk '$1 == \"Count\" && $2 == \"Eager\" && $5 == ".SIZE."' configuration_run_0.txt" using 4:6 ls 3 pt 3,\
    "< awk '$1 == \"Clock\" && $2 == \"Eager\" && $5 == ".SIZE."' configuration_run_0.txt" using 4:6 ls 4 pt 4,\
    "< awk '$1 == \"Count\" && $2 == \"Delayed\" && $5 == ".SIZE."' configuration_run_0.txt" using 4:6 ls 7 pt 1,\
    "< awk '$1 == \"Clock\" && $2 == \"Delayed\" && $5 == ".SIZE."' configuration_run_0.txt" using 4:6 ls 6 pt 6

set title "100k"
set size 0.5,1
set origin 1,0
set rmargin at screen LEFT
set lmargin at screen LEFT-DX
SIZE = 100000
plot "< awk '$1 == \"Count\" && $2 == \"Direct\" && $5 == ".SIZE."' configuration_run_0.txt" using 4:6 ls 1 pt 8,\
    "< awk '$1 == \"Clock\" && $2 == \"Direct\" && $5 == ".SIZE."' configuration_run_0.txt" using 4:6 ls 2 pt 2,\
    "< awk '$1 == \"Count\" && $2 == \"Eager\" && $5 == ".SIZE."' configuration_run_0.txt" using 4:6 ls 3 pt 3,\
    "< awk '$1 == \"Clock\" && $2 == \"Eager\" && $5 == ".SIZE."' configuration_run_0.txt" using 4:6 ls 4 pt 4,\
    "< awk '$1 == \"Count\" && $2 == \"Delayed\" && $5 == ".SIZE."' configuration_run_0.txt" using 4:6 ls 7 pt 1,\
    "< awk '$1 == \"Clock\" && $2 == \"Delayed\" && $5 == ".SIZE."' configuration_run_0.txt" using 4:6 ls 6 pt 6

#<null>
#here we need to unset everything that was set previously
unset origin
unset border
unset tics
unset label
unset arrow
unset title
unset object
set rmargin 0.05
set lmargin 0.05
#Now set the size of this plot to something BIG
set size 1,3 #however big you need it

#example key settings
set key horizontal reverse width -2 maxrows 2 maxcols 12 
set key at screen 0.5,screen 0.10 center top

#We need to set an explicit xrange.  Anything will work really.
set xrange [-1:1]
set yrange [-1:1]

plot \
1/0 lt -1 pi -4 pt 6 title "Count,Direct",\
NaN ls 2 title "Clock,Direct",\
NaN ls 3 title "Count,Eager",\
NaN ls 4 title "Clock,Eager",\
NaN ls 7 title "Count,Delayed",\
NaN ls 6 title "Clock,Delayed"
#</null>