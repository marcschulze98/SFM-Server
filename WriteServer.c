#include "SFM_Server.h"
///@file
///@brief Sends messages to the connected user
static void print_newest_message(struct arguments* args);
_Thread_local bool again = false;


void* writeserver_thread_func(void* arg)
{
	struct arguments* args = arg;
	struct return_info return_codes;
	struct timespec timer;
	timer.tv_sec = 0;
	timer.tv_nsec = 200000000L;

	
	(users+args->user_id)->write_connected = true;
	
	while(true)
	{
		print_newest_message(args);
		if(again)
			again = false;
		else
			nanosleep(&timer, NULL);
		return_codes = send_string(&test_connection, args->socket_fd);
		if(return_codes.error_occured)
			break;
	}

	atomic_store(&(users+args->user_id)->write_connected, false);
	printf("writeshutdown\n");
	pthread_exit(0);
}

static void print_newest_message(struct arguments* args)
{
	pthread_mutex_lock(&(users+args->user_id)->messages->mutex);
	
	struct string* message = dynamic_array_at((users+args->user_id)->messages,0);
	if(message != NULL)
	{
		again = true;
		//~ printf("Message for %s:\nTimestamp: %" PRId64", Message: %s, Length: %d\n", args->name, *(int64_t*)(message->data), message->data+8, message->length);
		printf("message: %s\n", message->data);
		convert_string(message);
		
		send_string(message, args->socket_fd);
		free(message->data);
		dynamic_array_remove((users+args->user_id)->messages,0);
	}
	
	pthread_mutex_unlock(&(users+args->user_id)->messages->mutex);
}
