CFLAGS	= -D_DEFAULT_SOURCE -std=c11 -Wall -Wextra -O3 -flto -s
LDFLAGS	= -flto
LIBS	= -ljansson -lsc68

sndh2raw: sndh2raw.c
	gcc $(CFLAGS) $(LDFLAGS) -o $@ $< $(LIBS)

clean:
	rm -f sndh2raw
