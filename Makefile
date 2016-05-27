all:
	gcc -o genPass -g main.c -lssl -lcrypto
clean:
	rm genPass
