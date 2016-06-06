all:
	gcc -o gen-pass -g main.c -lssl -lcrypto
clean:
	rm gen-pass
