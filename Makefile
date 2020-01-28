scrib: scrib.c
    $(CC) scrib.c -o scrib -Wall -Wextra -pedantic -std=c99

key: key.c
    $(CC) key.c -o key -Wall -Wextra -pedantic -std=c99

exp: exp.c
    $(CC) exp.c -o exp -Wall -Wextra -pedantic -std=c99

clean:
	rm -rf *.o scrib
