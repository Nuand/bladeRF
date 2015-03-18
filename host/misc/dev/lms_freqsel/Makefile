CFLAGS := -Wall -Wextra -ggdb3
LDFLAGS := -lm

all: freqsel

%: %.c
	$(CC) $(CFLAGS) $< $(LDFLAGS) -o $@

clean:
	rm -f freqsel

.PHONY: clean
