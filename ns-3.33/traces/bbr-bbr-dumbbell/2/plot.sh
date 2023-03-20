#! /bin/sh
algo1=bbr
algo2=bbr
picname=bbr
file1=10.1.1.1_49153_10.1.3.2_5000
file2=10.1.4.2_49153_10.1.5.2_5000
output=${algo1}-${algo2}
gnuplot<<!
set key top right
set xlabel "time/s" 
set ylabel "goodput/bps"
set xrange [0:200]
set yrange [0:10000]
set grid
set term "png"
set output "${output}-goodput.png"
plot "${file1}_goodput.txt" u 1:2 title "${algo1}" with linespoints lw 2,\
"${file2}_goodput.txt" u 1:2 title "${algo2}" with linespoints lw 2,
set output
exit
!

gnuplot<<!
set key top right
set xlabel "time/s" 
set ylabel "inflight/packets"
set xrange [0:200]
set yrange [0:150]
set grid
set term "png"
set output "${output}-inflight.png"
plot "${file1}_inflight.txt" u 1:2 title "${algo1}" with lines lw 2,\
"${file2}_inflight.txt" u 1:2 title "${algo2}" with lines lw 2,
set output
exit
!

gnuplot<<!
set key top right
set xlabel "time/s" 
set ylabel "rtt/ms"
set xrange [0:200]
set yrange [0:500]
set grid
set term "png"
set output "${output}-rtt.png"
plot "${file1}_rtt.txt" u 1:2 title "${algo1}" with lines lw 2,\
"${file2}_rtt.txt" u 1:2 title "${algo2}" with lines lw 2,
set output
exit
!

gnuplot<<!
set key top right
set xlabel "time/s" 
set ylabel "sendrate/bps"
set xrange [0:200]
set yrange [0:7000]
set grid
set term "png"
set output "${output}-sendrate.png"
plot "${file1}_sendrate.txt" u 1:2 title "${algo1}" with lines lw 2,\
"${file2}_sendrate.txt" u 1:2 title "${algo2}" with lines lw 2,
set output
exit
!