mt:
	gcc -c clientx.c
	gcc -c servery.c
	gcc -c queue.c
	gcc -c serverz.c
	gcc -c common.c

	gcc common.o clientx.o -o client
	gcc -pthread common.o queue.o serverz.o servery.o -lrt -o serverY
