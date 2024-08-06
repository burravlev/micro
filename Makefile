micro: micro.c
	$(CC) -o micro micro.c -Wall -W -pedantic -std=c99

clean:
	rm micro