micro: src/micro.c
	$(CC) -o micro src/micro.c -Wall -W -pedantic -std=c99

clean:
	rm micro