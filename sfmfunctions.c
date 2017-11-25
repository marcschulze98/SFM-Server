#include "SFM_Server.h"


inline void realloc_write(struct string* target, char c, uint_fast32_t offset) //write char into target->data, increase buffer-size and
{
	if((target->length * 2 + 2) >= UINT_FAST32_MAX)
	{
		printf("Fataler Fehler: Versuchte Nachricht zu schreiben die größer ist als 32-bit\n");
		exit(15);
	}
																//length accordingly if necessary (have enough space for next char)
	target->data[offset] = c;
	if(offset+1 >= target->length)
	{
		if((target->length * 2 + 2) < UINT32_MAX)
		{
			target->data = realloc(target->data, target->length * 2 + 2);
			target->length = target->length * 2;
		} else {
			target->data = realloc(target->data, UINT32_MAX);
			target->length = UINT32_MAX;
		}
	}
}

inline void reset_string_size(struct string* stringbuffer, uint_fast32_t buffer_size) //reset buffer to default size if needed
{
	if(stringbuffer->length != buffer_size)
	{
		//~ printf("realloced %ld, to %ld\n\n", stringbuffer->length, buffer_size);
		stringbuffer->data = realloc(stringbuffer->data, buffer_size);
		stringbuffer->length = buffer_size;
	}
}

inline char* convert_string(char* source)
{
	uint16_t length = (uint16_t)(strlen(source)+1);
	char* return_string = malloc(length+2u);

	return_string[0] = (char)(length >> 8);		//write length in reversed endianess in the first two bytes
	return_string[1] = (char)(length >> 0);

	strcpy(return_string+2,source);

	free(source);
	return return_string;
}

inline void printBits(size_t size, void* ptr)	//only for debugging
{
    unsigned char *b = (unsigned char*) ptr;
    unsigned char byte;
    size_t i;
    int j;

    for (i=0; i < size;i++)
    {
        for (j=7;j>=0;j--)
        {
            byte = (unsigned char)((b[i] >> j) & 1);
            printf("%u", byte);
        }
        puts("\n");
    }
    puts("");
}
inline bool get_message(struct string* message, int socket_fd) //create buffer for realloc_read and read the first two bytes for length of incoming string, then run realloc_read to read that amount
{
	struct pollfd socket_ready = { .fd = socket_fd, .events = POLLIN, .revents = 0};
	uint16_t bytes_to_read = 0;
	uint_fast32_t offset = 0;
	bool got_message = false;

	while(poll(&socket_ready, 1, POLL_TIMEOUT)) //poll socket input and check for interrupt
	{
		got_message = true;
		long read_return;
		if((read_return = read(socket_fd, &bytes_to_read, 2)) != 2)
		{
			if(read_return == -1)
			{
				printf("Fehler beim Lesen der eingehenden Nachricht: %s\n", strerror(errno));
				exit(3);
			}
			printf("Eingehende Nachricht besitzt ungültiges Format, Client wird geschlossen...\n");
			exit(3);
		}
		
		if(!IS_BIG_ENDIAN)
		{
			swap_endianess_16(&bytes_to_read);
		}
		
		if(bytes_to_read == 0)
		{
			printf("Fataler Fehler: Nachrichtenlänge zu kurz\n");
			exit(14);
		}

		if(realloc_read(message, bytes_to_read, socket_fd, offset))	//message.data is a null terminated string / if realloc_read returns true, then message continues
		{
			offset += (uint16_t)(bytes_to_read-2);
			continue;
		}
	}

	return got_message;
}


inline bool realloc_read(struct string* target, unsigned short bytes_to_read, int socket_fd, uint_fast32_t offset) 	//if buffer is too small to hold bytes_to_read then double
{																				//the space until space is big enough, null terminate the string
	long bytes_read = 0;

	adjust_string_size(target, bytes_to_read+offset);

	if((bytes_read = read(socket_fd, (target->data+offset), bytes_to_read)) == -1)
	{
		printf("Fehler beim Empfang von Nachrichten: %s\n", strerror(errno));
	} else if(bytes_read != bytes_to_read) {
		printf("Fataler Fehler: Sollte %d Bytes lesen, aber es kamen nur %ld Bytes an\n",bytes_to_read, bytes_read);
	}

	if(target->data[bytes_read-1] != '\0') 		//last char has to be nul
	{
		target->data[bytes_read-1] = '\0';
	}

	return target->data[bytes_read-2] == '\0' ? true : false; //if the char one before last is nul, then the message is split
}


inline void swap_endianess_16(uint16_t * byte) //swap 16 bytes in endianess
{
	uint16_t swapped = (uint16_t)((*byte>>8) | (*byte<<8));
	*byte = swapped;
}


inline void adjust_string_size(struct string* target ,uint_fast32_t size)
{
	while(size >= target->length)
	{
		//~ printf("realloced size: %d, target->length: %ld\n\n", size, target->length);
		target->data = realloc(target->data, target->length * 2);
		target->length = target->length * 2;
	}
}

void free_linked_list_pointer(struct linked_list* currentitem)
{
	struct linked_list tmp;
	while(currentitem != NULL)
	{
		tmp = *currentitem;
		free(currentitem->data);
		free(currentitem);
		currentitem = tmp.next;
	}
}
