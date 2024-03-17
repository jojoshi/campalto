CFLAGS = -O0 -Wall -Wextra -lwayland-server -lwlroots -Wpedantic -std=c11 -D_XOPEN_SOURCE -DWLR_USE_UNSTABLE -I/usr/include/libdrm -I/usr/include/pixman-1
PROGNAME = "campalto"

SRC=$(wildcard src/*.c)

all: main

main: $(SRC)
	$(CC) $(CFLAGS) -g -DDebug -o $(PROGNAME) -Isrc/ $^

prod: $(SRC)
	$(CC) $(CFLAGS) -o $(PROGNAME)_prod -Isrc/ $^

asan: $(SRC)
	$(CC) $(CFLAGS) -g -DDebug -fsanitize=address -o $(PROGNAME) -Isrc/ $^

clean:
	rm -f $(PROGNAME)