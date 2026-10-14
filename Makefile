
CC = gcc
CFLAGS = -Wall -Wextra -Werror -std=c11 -D_FILE_OFFSET_BITS=64 -Iinclude

TARGET = raidvol
REBUILD_TARGET = rebuild_test
BENCHMARK_TARGET = benchmark
METADATA_TARGET = metadata_test
SOURCES = src/main.c src/raidvol.c
OBJECTS = $(SOURCES:.c=.o)

all: $(TARGET) $(REBUILD_TARGET) $(BENCHMARK_TARGET) $(METADATA_TARGET)
$(TARGET): $(OBJECTS)
	$(CC) $(CFLAGS) $(OBJECTS) -o $(TARGET)

$(REBUILD_TARGET): tests/rebuild_test.c src/raidvol.c include/raidvol.h
	$(CC) $(CFLAGS) tests/rebuild_test.c src/raidvol.c -o $(REBUILD_TARGET)

$(BENCHMARK_TARGET): tests/benchmark.c src/raidvol.c include/raidvol.h
	$(CC) $(CFLAGS) tests/benchmark.c src/raidvol.c -o $(BENCHMARK_TARGET)
$(METADATA_TARGET): tests/metadata_test.c src/raidvol.c include/raidvol.h
	$(CC) $(CFLAGS) tests/metadata_test.c src/raidvol.c -o $(METADATA_TARGET)
src/%.o: src/%.c include/raidvol.h
	$(CC) $(CFLAGS) -c $< -o $@

test: all
	./$(TARGET)
	./$(REBUILD_TARGET)
	./$(BENCHMARK_TARGET)
	./$(METADATA_TARGET)
clean:
	rm -f $(OBJECTS) $(TARGET) $(REBUILD_TARGET) $(BENCHMARK_TARGET) $(METADATA_TARGET)
.PHONY: all test clean
