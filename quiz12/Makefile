CFLAGS = -Wall -Werror -g -O2 -I.
CFLAGS += -fsanitize=thread

spsc_test: spsc_test.c spsc_queue.h
	gcc ${CFLAGS} spsc_test.c -o  spsc_test -lpthread

clean:
	rm -f spsc_test
