CC = gcc
CFLAGS = -Wall -Wextra -O2
TARGET = cipher-core
SOURCES = alpha.c beta.c gama.c delta.c epsilon.c zeta.c eta.c theta.c

all: $(TARGET)

$(TARGET): $(SOURCES)
	$(CC) $(CFLAGS) -o $(TARGET) $(SOURCES)

clean:
	rm -f $(TARGET) *.o

run: $(TARGET)
	./$(TARGET)

.PHONY: all clean run
