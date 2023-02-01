all: prompt.c
	$(CC) -std=c99 -Wall prompt.c mpc.c -ledit -lm -o prompt
clean:
	rm -f prompt
