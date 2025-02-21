CC ?= gcc
CFLAGS = -Wall -Wextra -Wno-unused-parameter

ifeq ($(DEBUG), 1)
	CFLAGS += -fsanitize=address,undefined -g
else
	CFLAGS += -O3
endif

all: dictbuilder

dictbuilder: dictbuilder.c ./hashmap.c/hashmap.h ./hashmap.c/hashmap.c
	$(CC) ./hashmap.c/hashmap.c dictbuilder.c -o dictbuilder -lm $(CFLAGS)
clean:
	rm dictbuilder -f
