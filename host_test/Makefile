CC ?= cc
CFLAGS ?= -std=c11 -Wall -Wextra -Werror -O1 -DCK_HOST_TEST
SRC = ../src/codex_keymap.c
TEST = codex_keymap_test.c

.PHONY: test clean

test: /tmp/codex_keymap_test
	/tmp/codex_keymap_test

/tmp/codex_keymap_test: $(TEST) $(SRC) ../src/codex_keymap.h
	$(CC) $(CFLAGS) -o $@ $(TEST) $(SRC)

clean:
	rm -f /tmp/codex_keymap_test
