
CC = gcc
CFLAGS = -Wall
LDFLAGS = -pthread
TARGET = tmod_tester

all: $(TARGET)

$(TARGET): $(TARGET).c
	$(CC) -o $@ $^ $(CFLAGS) $(LDFLAGS)

clean:
	-@ $(RM) *.o $(TARGET)
