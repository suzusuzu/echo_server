#!/bin/bash

make all

for file in `ls *.c`; do
    BIN=$(echo $file | sed s/\\.c//g)
    echo $BIN
    ./$BIN > /dev/null &
    PID=$!
    for i in $(seq 1 1000); do
        RET=$(echo $i | nc -q 0 localhost 8080)
        if [ "$RET" != "$i" ]; then
            echo "no pass test$i in $BIN"
            kill -9 $PID
            exit 1
        fi
    done
    kill -9 $PID > /dev/null
done

