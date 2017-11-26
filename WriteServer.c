#include "SFM_Server.h"

static void loop(struct arguments* args);
static void print_newest_message(struct arguments* args);

void* writeserver_thread_func(void* arg)
{
	struct arguments* args = arg;
	struct string test_connection = { .data = "\x00\x01\0", .length = 3};
	
	
	(users+args->user_id)->write_connected = true;
	
	while(true)
	{
		loop(args);
		sleep(1);
		if(!send_string(&test_connection, args->socket_fd))
		{
			break;
		}
	}

	(users+args->user_id)->write_connected = false;
	printf("writeshutdown\n");
	pthread_exit(0);
}

static void loop(struct arguments* args)
{
	print_newest_message(args);
}

static void print_newest_message(struct arguments* args)
{
	struct linked_list* current = (users+args->user_id)->messages;
	pthread_mutex_lock(&current->mutex);

	if(current->next != NULL)
	{
		current = current-> next;
		pthread_mutex_lock(&current->mutex);
	
		struct string* message = current->data;
		convert_string(message);
		
		
		printf("Message for %s:\nTimestamp: %ld, Message: %s, Length: %d\n", args->name, *(int64_t*)(message->data+2), message->data+11, message->length);
		send_string(message, args->socket_fd);
		
		(users+args->user_id)->messages->next = current->next;
		pthread_mutex_destroy(&current->mutex);
		free(message->data);
		free(current->data);
		free(current);
	}

	pthread_mutex_unlock(&current->mutex);
}
