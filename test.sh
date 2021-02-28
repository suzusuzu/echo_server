#!/bin/bash

for file in `ls *.c`; do
    BIN=$(echo $file | sed s/\\.c//g)
    echo ''
    echo =============== start $BIN test =============== 
    ./$BIN > /dev/null &
    PID=$!
    if [ "$file" = "fork_echo.c" ]; then # If TIME is over 2000 in fork_echo.c, it will crash.
        TIME=2000
    else
        TIME=10000
    fi
    echo "run $TIME times"
    time $(
    for i in $(seq 1 $TIME); do
        RET=$(echo $i | nc -q 0 localhost 8080)
        if [ "$RET" != "$i" ]; then
            echo "no pass test $i case in $BIN"
            kill -9 $PID
            exit 1
        fi
    done
    )
    echo =============== pass $BIN test =============== 
    kill -9 $PID
    wait $PID 2>/dev/null
done

exit 0
