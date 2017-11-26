all:
	gcc -m64 -std=c11 ConnectServer.c ListenServer.c WriteServer.c ../SFM_Common_C/sfmfunctions.c Cleanup.c sfmserverfunctions.c -lm -lpthread -o ConnectServer \
	-Og -g -flto -ffast-math -fstrict-aliasing -funroll-loops -no-pie -fsanitize=address \
	-Wall -Wextra -Wpedantic -Wnull-dereference -Wshadow -Wconversion -Wstrict-prototypes -Wmissing-prototypes -Wcast-qual -Wstrict-overflow=5 -Wunreachable-code -Wno-unused-parameter \
	#-fsanitize=address
