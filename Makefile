all: prompt.c
	$(CC) -std=c99 -Wall prompt.c -ledit -o prompt
clean:
	rm -f prompt
