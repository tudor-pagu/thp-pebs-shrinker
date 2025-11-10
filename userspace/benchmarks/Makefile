CFLAGS=-O2 -Wall
BENCHES=bench_thp_basic bench_split_trigger bench_promotion

all: $(BENCHES)

%: %.c
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm -f $(BENCHES)
