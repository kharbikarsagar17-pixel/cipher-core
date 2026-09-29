CC = gcc
CFLAGS = -Wall -Wextra -O2 -std=c99
LDFLAGS = -lm -lpthread
TARGET = cipher-core
SOURCES = alpha.c beta.c gama.c delta.c epsilon.c zeta.c eta.c theta.c \
          iota.c kappa.c lambda.c mu.c nu.c xi.c omicron.c pi.c rho.c sigma.c tau.c

all: $(TARGET)

$(TARGET): $(SOURCES)
	$(CC) $(CFLAGS) -o $(TARGET) $(SOURCES) $(LDFLAGS)

clean:
	rm -f $(TARGET) *.o

run: $(TARGET)
	./$(TARGET)

.PHONY: all clean run
