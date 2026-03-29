CC = gcc
INCLUDES = -I../include -I../include/microposix -I../include/microposix/kernel -I../include/microposix/mm -I../include/microposix/debug -I../include/microposix/hal
CFLAGS = -Wall -Wextra -g $(INCLUDES) -DMICROPOSIX_PROFILE=1

SRC = ../src/kernel/scheduler.c ../src/kernel/thread.c ../src/kernel/ipc.c ../src/mm/pool.c ../src/debug/log.c
OBJ = $(SRC:.c=.o)

TEST_SRC = test_scheduler.c
TEST_EXE = test_scheduler

all: $(TEST_EXE)
	./$(TEST_EXE)

$(TEST_EXE): $(TEST_SRC) $(SRC)
	$(CC) $(CFLAGS) $^ -o $@

clean:
	rm -f $(TEST_EXE) ../src/kernel/*.o ../src/mm/*.o ../src/debug/*.o

.PHONY: all clean
