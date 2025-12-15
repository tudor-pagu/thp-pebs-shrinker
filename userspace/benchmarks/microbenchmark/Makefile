CC ?= gcc
CFLAGS  ?= -O2 -Wall -Wextra -Wshadow -Wformat=2 -Wconversion -Wno-sign-conversion
TARGET  := bench_rand_access
SRC     := bench_rand_access.c

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

clean:
	rm -f $(TARGET)

# convenience
run: $(TARGET)
	./$(TARGET) -n 30 -b 20000 -p 200 -t 30 --huge
