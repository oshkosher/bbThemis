set title "IO Throughput of MaskRCNN over time"
set xlabel "Seconds from start of job"
set ylabel "IO throughput in MiB per second"


set style fill solid 1.0 border -1
set boxwidth .8 relative

plot \
  "foo" using ($1):(0):(0):($3/1048576):($3/1048576) with candlestick fill solid linestyle -1 title "Read", \
  "" using ($1):($3/1048576):($3/1048576):(($3+$7)/1048576):(($3+$7)/1048576) with candlestick fill empty linestyle -1 title "Write"
