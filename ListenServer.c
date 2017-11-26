#include "SFM_Server.h"

static void loop(struct string_info* info, struct arguments* args);
static void get_string_info(struct string_info* info);
static void sort_message(const struct string_info* info);
static void put_message_extern(const struct string_info* info);
static void put_message_local(const struct string_info* info);
static bool handle_command(const struct string_info* info);

bool should_shutdown = false;

void* listenserver_thread_func(void* arg)
{
	struct arguments* args = arg;
	struct string message = { .data = malloc(DEFAULT_BUFFER_LENGTH), .length = 0, .capacity = DEFAULT_BUFFER_LENGTH };
	struct string_info info = { .source_server = this_server_name, .source_user = args->name, .timestamp = 0, .message = &message};
	
	
	(users+args->user_id)->listen_connected = true;


	while(!should_shutdown)
	{
		loop(&info, args);
	}

	(users+args->user_id)->listen_connected = false; 	// set status to offline for cleanupthread
	free(info.message->data);
	printf("listenshutdown\n");
	pthread_exit(0);
}

static void loop(struct string_info* info, struct arguments* args)
{
	while(!get_message(info->message,args->socket_fd)); 		//loop until we get a message

	if(valid_message_format(info->message, false)) 			//chech for valid message (command or short-message format)
	{
		printf("Received valid message from: %s -> %s\n", args->name, info->message->data);
		get_string_info(info); 	//put source information as prefix
		if(!handle_command(info)) 					//check if command and handle it accordingly
		{
			sort_message(info);					//put message in right queue
		}
	} else {
		printf("Received invalid message from: %s -> %s\n", args->name, info->message->data);
	}
}

static void get_string_info(struct string_info* info) 				//adds source-prefixes + timestamp
{
	struct timespec ts;
	timespec_get(&ts, TIME_UTC);
	int64_t time = ts.tv_sec;
	info->timestamp = time;
		
	for(uint32_t i = 0; i <= info->message->length; i++)
	{
		if(info->message->data[i] == ':')
		{
			info->message_begin = i+1;
			break;
		}
	}
}

static void sort_message(const struct string_info* info) //check target server and sort message internally or externally
{
	char* target_server = "\0";

	for(uint32_t i = 0; i <= info->message->length; i++)
	{
		if(info->message->data[i] == '@')
		{
			target_server = malloc(i+1);
			memcpy(target_server, info->message->data, i);
			target_server[i] = '\0';
			break;
		}
	}

	if(strcmp(target_server, this_server_name) != 0)
	{
		put_message_extern(info);
	} else {
		put_message_local(info);
	}

	if(*target_server != '\0')
	{
		free(target_server);
	}
}

static void put_message_extern(const struct string_info* info)
{

}

static void put_message_local(const struct string_info* info) //copy message in queue of the target user
{
	uint32_t server_found = 0;
	uint32_t user_id = 0;
	char* username = NULL;
	
	for(uint32_t i = 0; i < info->message->length; i++)
	{
		if(server_found && (info->message->data[i] == ':'))
		{
			username = malloc(i-server_found);
			memcpy(username, info->message->data+server_found, i-server_found);
		}
		if(info->message->data[i] == '@')
		{
			server_found = i+1;
		}
	}
	
	if((user_id = get_user_id(username)) == 0)
	{
		return;
	}
	
	struct linked_list* current = (users+user_id)->messages;
	pthread_mutex_t* old_lock = &current->mutex;
	pthread_mutex_lock(old_lock);

	while(current->next != NULL)
	{
		current = current-> next;
		pthread_mutex_lock(&current->mutex);
		pthread_mutex_unlock(old_lock);
		old_lock = &current->mutex;
	}

	current->next = malloc(sizeof(*current));
	current = current-> next;

	pthread_mutex_init(&current->mutex, NULL);
	current->next = NULL;

	uint32_t complete_length = (uint32_t)(strlen(info->source_server) + strlen(info->source_user) + strlen(info->message->data + info->message_begin) + 8 + 3 + 1);
	current->data = malloc(sizeof(struct string));
	struct string* message = current->data;
	
	message->data = malloc(DEFAULT_BUFFER_LENGTH);
	message->length = 0;
	message->capacity = DEFAULT_BUFFER_LENGTH;
	adjust_string_size(current->data, complete_length);
	
	memcpy(message->data + message->length, &info->timestamp, 8);
	message->length += 8;
	message->data[message->length] = '>';
	message->length += 1;
	
	strcpy(message->data + message->length, info->source_server);
	message->length += (uint32_t)strlen(info->source_server);
	message->data[message->length] = '@';
	message->length += 1;
	
	strcpy(message->data + message->length, info->source_user);
	message->length += (uint32_t)strlen(info->source_user);
	message->data[message->length] = ':';
	message->length += 1;
	
	strcpy(message->data + message->length, info->message->data + info->message_begin);
	message->length += (uint32_t)strlen(info->message->data + info->message_begin);
	message->data[message->length] = '\0';
	message->length += 1;
		
	//~ printf("Orig: Timestamp: %ld, Message: %s\n", info->timestamp, info->message->data);
	printf("New:  Timestamp: %ld, Message: %s\n", *(int64_t*)message->data, message->data+9);

	pthread_mutex_unlock(old_lock);
}

static bool handle_command(const struct string_info* info)
{
	char* command = info->message->data + info->message_begin;
	
	if(strcmp(command, "/bye") == 0)
	{
		should_shutdown = true;
		printf("got /bye\n");
		return true;
	}
	
	return false;
}
