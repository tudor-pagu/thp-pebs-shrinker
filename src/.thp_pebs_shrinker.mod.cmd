savedcmd_thp_pebs_shrinker.mod := printf '%s\n'   thp_pebs_shrinker.o | awk '!x[$$0]++ { print("./"$$0) }' > thp_pebs_shrinker.mod
