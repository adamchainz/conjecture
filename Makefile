CFLAGS?= --std=c99 -I. -D_XOPEN_SOURCE -Wall -pedantic -Werror -pg -fPIC

clean:
	rm -rf *.o

reformat:
	clang-format -i conjecture.c conjecture.h examples/*.c

conjecture.o: conjecture.c
	$(CC) -c $(CFLAGS) conjecture.c

conjecture.so: conjecture.o
	$(CC) conjecture.o --shared -oconjecture.so

examples/summing: conjecture.o examples/summing.c
	$(CC) $(CFLAGS) conjecture.o examples/summing.c -o examples/summing

examples/flaky: conjecture.o examples/flaky.c
	$(CC) $(CFLAGS) conjecture.o examples/flaky.c -o examples/flaky

examples/summing_many: conjecture.o examples/summing_many.c
	$(CC) $(CFLAGS) conjecture.o examples/summing_many.c -o examples/summing_many

examples/reverse_summing_doubles: conjecture.o examples/reverse_summing_doubles.c
	$(CC) $(CFLAGS) conjecture.o examples/reverse_summing_doubles.c -o examples/reverse_summing_doubles

examples/associative_doubles: conjecture.o examples/associative_doubles.c
	$(CC) $(CFLAGS) conjecture.o examples/associative_doubles.c -o examples/associative_doubles

examples/novalid: conjecture.o examples/novalid.c
	$(CC) $(CFLAGS) conjecture.o examples/novalid.c -o examples/novalid

examples/unsorted_strings: conjecture.o examples/unsorted_strings.c
	$(CC) $(CFLAGS) conjecture.o examples/unsorted_strings.c -o examples/unsorted_strings

examples/knapsack: conjecture.o examples/knapsack.c
	$(CC) $(CFLAGS) conjecture.o examples/knapsack.c -o examples/knapsack



.PHONY: clean reformat
