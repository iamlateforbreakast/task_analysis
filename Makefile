all: read_file

read_file: read_file.c
	gcc -g read_file.c -o read_file

