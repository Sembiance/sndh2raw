CFLAGS	= -std=c11 -Wall -Wextra -O3 -flto -s $(shell sc68-config --cflags)
LDFLAGS	= -flto
LIBS	= -ljansson $(shell sc68-config --libs)

sndh2raw: sndh2raw.c
	gcc $(CFLAGS) $(LDFLAGS) -o $@ $< $(LIBS)
