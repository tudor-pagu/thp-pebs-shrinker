#!/usr/bin/env bash

cd "$(dirname "$0")" # move to directory where file is

(
cd src
make
rsync -e "ssh -p 2222" ./thp_pebs_shrinker.ko arch@127.0.0.1:/home/arch/thp_pebs_shrinker.ko
ssh -p 2222 arch@127.0.0.1 "echo arch | sudo -S rmmod thp_pebs_shrinker"
ssh -p 2222 arch@127.0.0.1 "echo arch | sudo -S insmod thp_pebs_shrinker.ko"
)



