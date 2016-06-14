CC = gcc
SANITIZER = -fsanitize=address 
THREAD_LIBS = -lpthread
CRYPTO_LIBS = -lcrypto
GLIB = `pkg-config --cflags --libs glib-2.0`
 
all: main

main: main.c main.h
	$(CC) -o main main.c $(CRYPTO_LIBS) $(THREAD_LIBS) $(GLIB) $(SANITIZER) 

 