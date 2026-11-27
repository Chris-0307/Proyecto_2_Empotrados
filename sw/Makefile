CC = arm-linux-gnueabihf-gcc
CFLAGS = -O2 -Wall -Wextra -static

player: player.c
	$(CC) $(CFLAGS) $< -o $@

clean:
	rm -f player
