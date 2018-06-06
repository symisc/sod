# SOD does not generally require a Makefile to build. Just drop sod.c and its accompanying
# header files on your source tree and you are done.
CC = clang
CFLAGS = -lm -Ofast -march=native -Wall -std=c99

sod: sod.c
	$(CC) sod.c samples/cnn_face_detection.c -o sod_face_detect -I. $(CFLAGS)