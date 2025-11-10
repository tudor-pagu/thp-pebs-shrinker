# log_thp_stats.sh
#!/bin/bash
out=${1:-thp_log.txt}
echo "timestamp,AnonHugePages(kB),ShmemHugePages(kB)" > $out
while true; do
    ts=$(date +%s)
    anon=$(grep -i AnonHuge /proc/meminfo | awk '{print $2}')
    shmem=$(grep -i ShmemHuge /proc/meminfo | awk '{print $2}')
    echo "$ts,$anon,$shmem" >> $out
    sleep 0.2
done
