CFLAGS?= --std=c99 -I. -D_XOPEN_SOURCE -Wall -pedantic -Werror -pg

clean:
	rm -rf *.o

conjecture.o: conjecture.c
	$(CC) -c $(CFLAGS) conjecture.c

examples/summing: conjecture.o examples/summing.c
	$(CC) $(CFLAGS) conjecture.o examples/summing.c -o examples/summing

examples/summing_many: conjecture.o examples/summing_many.c
	$(CC) $(CFLAGS) conjecture.o examples/summing_many.c -o examples/summing_many
