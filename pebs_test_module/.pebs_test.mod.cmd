savedcmd_pebs_test.mod := printf '%s\n'   pebs_test.o | awk '!x[$$0]++ { print("./"$$0) }' > pebs_test.mod
