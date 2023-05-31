#!/bin/bash

# 可執行檔案的路徑和名稱
EXECUTABLE="./waf"

# 定義參數
CC1="cubic"
CC2="bbr"
FOLDER="cubic-bbr-2"

# 迭代執行命令並更改--it參數值
for ((it=1; it<=14; it++))
do
    COMMAND="$EXECUTABLE --run \"scratch/tcp-dumbbell --it=$it --cc1=$CC1 --cc2=$CC2 --folder=$FOLDER\""
    eval $COMMAND
done
