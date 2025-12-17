CC = gcc
CFLAGS = -Wall
LDFLAGS = -lpfm

TARGET = pebs_event

all: $(TARGET)

$(TARGET): pebs_event.c
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

clean:
	rm -f $(TARGET)
