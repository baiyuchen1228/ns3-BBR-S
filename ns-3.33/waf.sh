for loop in in $(seq 13 14)
do
    ./waf --run "scratch/tcp-dumbbell --it=loop --cc1=cubic --cc2=bbr --folder=cubic-bbr"
done