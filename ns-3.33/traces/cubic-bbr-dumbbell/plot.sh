#!/bin/sh
algo1=Cubic
algo2=bbr
picname=bbr
file1=10.1.1.1_49153_10.1.3.2_5000
file2=10.1.1.1_49154_10.1.3.2_5000
# file2=10.1.4.2_49153_10.1.5.2_5000
output=${algo1}-${algo2}
for i in $(seq 1 5)
do

    folder="$i"

    gnuplot<<EOF
    set key top right
    set xlabel "time/s" 
    set xrange [0:200]
    set grid
    set term "png"

    set output "${folder}/${output}-goodput.png"
    set ylabel "goodput/bps"
    set yrange [0:10000]
    plot "${folder}/${file1}_goodput.txt" u 1:2 title "${algo1}" with linespoints lw 2,\
         "${folder}/${file2}_goodput.txt" u 1:2 title "${algo2}" with linespoints lw 2
    set output

    set output "${folder}/${output}-goodput-MA.png"
    set ylabel "goodput-MA/bps"
    set yrange [0:10000]
    plot "${folder}/1_2_goodput_MA.txt" u 1:2 title "${algo1}" with linespoints lw 2,\
         "${folder}/4_5_goodput_MA.txt" u 1:2 title "${algo2}" with linespoints lw 2
    set output

    set output "${folder}/${output}-inflight.png"
    set ylabel "inflight/packets"
    set yrange [0:150]
    plot "${folder}/${file1}_inflight.txt" u 1:2 title "${algo1}" with lines lw 2,\
         "${folder}/${file2}_inflight.txt" u 1:2 title "${algo2}" with lines lw 2
    set output

    set output "${folder}/${output}-rtt.png"
    set ylabel "rtt/ms"
    set yrange [0:500]
    plot "${folder}/${file1}_rtt.txt" u 1:2 title "${algo1}" with lines lw 2,\
         "${folder}/${file2}_rtt.txt" u 1:2 title "${algo2}" with lines lw 2
    set output

    set output "${folder}/${output}-rtt-MA.png"
    set ylabel "rtt-MA/ms"
    set yrange [0:500]
    plot "${folder}/1_2_rtt_MA.txt" u 1:2 title "${algo1}" with lines lw 2,\
         "${folder}/4_5_rtt_MA.txt" u 1:2 title "${algo2}" with lines lw 2
    set output

    set output "${folder}/${output}-sendrate.png"
    set ylabel "sendrate/bps"
    set yrange [0:7000]
    plot "${folder}/${file1}_sendrate.txt" u 1:2 title "${algo1}" with lines lw 2,\
         "${folder}/${file2}_sendrate.txt" u 1:2 title "${algo2}" with lines lw 2
    set output

    exit
EOF

done
