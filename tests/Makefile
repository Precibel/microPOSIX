CC = gcc
INCLUDES = -I../include -I../include/microposix -I../include/microposix/kernel -I../include/microposix/mm -I../include/microposix/debug -I../include/microposix/hal -I../include/microposix/serialization
CFLAGS = -Wall -Wextra -g $(INCLUDES) -DMICROPOSIX_PROFILE=1 -DMICROPOSIX_PLATFORM_POSIX=1

# Source files needed for tests
SRC = ../src/kernel/scheduler.c ../src/kernel/thread.c ../src/kernel/ipc.c \
      ../src/mm/pool.c ../src/mm/tlsf.c ../src/mm/memory_view.c \
      ../src/mm/shared_heap.c ../src/mm/gc.c \
      ../src/debug/log.c ../src/debug/shell.c ../src/debug/fault.c \
      ../src/serialization/json.c ../src/serialization/protobuf.c ../src/serialization/edf.c

OBJ = $(SRC:.c=.o)

# Test executables
TEST_SRC = test_scheduler.c test_memory.c test_serialization.c test_odata.c
TEST_EXES = $(TEST_SRC:.c=)

all: $(TEST_EXES)
	@echo "Running all tests..."
	@for test in $(TEST_EXES); do \
	    echo "Running $$test..."; \
	    ./$$test || exit 1; \
	    echo ""; \
	done
	@echo "All tests passed!"

# Build each test
%: %.c $(OBJ)
	$(CC) $(CFLAGS) $< -o $@ $(OBJ)

clean:
	rm -f $(TEST_EXES) $(OBJ)

.PHONY: all clean
