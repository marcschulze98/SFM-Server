all:
	musl-gcc -m64 -std=c11 *.c ../SFM_Common_C/sfmfunctions.c -lm -lpthread -o ConnectServer \
	-Og -g -flto -ffast-math -fstrict-aliasing -funroll-loops -no-pie \
	-Wall -Wextra -Wpedantic -Wnull-dereference -Wshadow -Wconversion -Wstrict-prototypes -Wmissing-prototypes -Wcast-qual -Wstrict-overflow=5 -Wunreachable-code -Wno-unused-parameter \
	#-fsanitize=address
