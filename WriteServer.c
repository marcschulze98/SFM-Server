#include "SFM_Server.h"

static void print_newest_message(struct arguments* args);

void* writeserver_thread_func(void* arg)
{
	struct arguments* args = arg;
	struct return_info return_codes;
	
	(users+args->user_id)->write_connected = true;
	
	while(true)
	{
		print_newest_message(args);
		sleep(1);
		return_codes = send_string(&test_connection, args->socket_fd);
		if(return_codes.error_occured)
		{
			break;
		}
	}

	(users+args->user_id)->write_connected = false;
	printf("writeshutdown\n");
	pthread_exit(0);
}

static void print_newest_message(struct arguments* args)
{
	pthread_mutex_lock(&(users+args->user_id)->messages->mutex);
	
	struct string* message = dynamic_array_at((users+args->user_id)->messages,0);
	if(message != NULL)
	{
		convert_string(message);
		
		printf("Message for %s:\nTimestamp: %ld, Message: %s, Length: %d\n", args->name, *(int64_t*)(message->data+2), message->data+11, message->length);
		send_string(message, args->socket_fd);
		free(message->data);
		dynamic_array_remove((users+args->user_id)->messages,0);
	}
	
	pthread_mutex_unlock(&(users+args->user_id)->messages->mutex);
}
