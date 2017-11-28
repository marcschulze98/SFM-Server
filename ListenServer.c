#include "SFM_Server.h"

static void loop(struct string_info* info, struct arguments* args);
static void get_string_info(struct string_info* info);
static void sort_message(const struct string_info* info);
static void put_message_extern(const struct string_info* info);
static void put_message_local(const struct string_info* info);
static void create_group(char* groupname, struct arguments* args);
static void delete_group(char* groupname, struct arguments* args);
static bool handle_command(const struct string_info* info, struct arguments* args);

bool should_shutdown;

void* listenserver_thread_func(void* arg)
{
	struct arguments* args = arg;
	struct string message = new_string(DEFAULT_BUFFER_LENGTH);
	struct string_info info = { .source_server = this_server_name, .source_user = args->name, .timestamp = 0, .message = &message};
	should_shutdown = false;
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
	//loop until we get a message
	struct return_info return_codes;
	do
	{
		return_codes = send_string(&test_connection, args->socket_fd);
		if(return_codes.error_occured)
		{
			should_shutdown = true;
			return;
		}
		sleep(3);
		return_codes = get_message(info->message,args->socket_fd);
		if(return_codes.error_occured)
		{
			should_shutdown = true;
			return;
		}

	}while(!return_codes.return_code); 

	if(valid_message_format(info->message, false)) 			//check for valid message (command or short-message format)
	{
		printf("Received valid message from: %s -> %s\n", args->name, info->message->data);
		get_string_info(info); 						//put source information as prefix
		if(!handle_command(info, args)) 					//check if command and handle it accordingly
		{
			sort_message(info);						//put message in right queue
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
			username = malloc(i-server_found+1);
			memcpy(username, info->message->data+server_found, i-server_found);
			username[i-server_found] = '\0';
			break;
		}
		if(info->message->data[i] == '@')
		{
			server_found = i+1;
		}
	}
	printf("User-name = %s\n", username);
	if((user_id = get_user_id(username)) == 0)
	{
		free(username);
		return;
	}
	free(username);
	
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
	//~ printf("New:  Timestamp: %ld, Message: %s\n", *(int64_t*)message->data, message->data+9);

	pthread_mutex_unlock(old_lock);
}

static bool handle_command(const struct string_info* info, struct arguments* args)
{
	char* command = info->message->data + info->message_begin;
	struct string string_without_payload = new_string(DEFAULT_NAME_LENGTH);
	bool command_run = false;

	for(uint32_t i = 0; i < strlen(command); i++)
	{
		if(command[i] != ' ')
		{
			realloc_write(&string_without_payload, command[i], i);
		} else {
			realloc_write(&string_without_payload, '\0', i);
			break;
		}
	}
	printf("%s|%d\n", string_without_payload.data, strcmp(string_without_payload.data, "/creategroup"));
	
	if(strcmp(string_without_payload.data, "/bye") == 0)
	{
		should_shutdown = true;
		printf("got /bye\n");
		command_run = true;
	} else if(strcmp(string_without_payload.data, "/creategroup") == 0) {
		create_group(command+13, args);
		command_run = true;
	} else if(strcmp(string_without_payload.data, "/deletegroup") == 0) {
		delete_group(command+13, args);
		command_run = true;
	} else if(strcmp(string_without_payload.data, "/addgroup") == 0) {
		command_run = true;
	}
	
	free(string_without_payload.data);
	
	return command_run;
}

static void create_group(char* groupname, struct arguments* args)
{
	if(strlen(groupname) == 0)
	{
		return;
	}
	
	struct linked_list* current = groups;
	struct group* current_group = current->data;

	pthread_mutex_t* old_lock = &current->mutex;
	pthread_mutex_lock(old_lock);
	
	while(current->next != NULL)
	{
		current = current-> next;
		current_group = current->data;
		if(current_group != NULL && strcmp(groupname, current_group->name) == 0)
		{
			break;
		}
		pthread_mutex_lock(&current->mutex);
		pthread_mutex_unlock(old_lock);
		old_lock = &current->mutex;
	}
	
	if(current_group != NULL && strcmp(groupname, current_group->name) == 0)
	{
		pthread_mutex_unlock(old_lock);
		return;
	}

	current->next = new_linked_list();
	current = current-> next;
	
	current->data = malloc(sizeof(struct group));
	current_group = current->data;
	
	current_group->master = malloc(strlen(args->name)+1);
	strcpy(current_group->master, args->name);
	current_group->name = malloc(strlen(groupname)+1);
	strcpy(current_group->name, groupname);
	
	current_group->members = new_linked_list();
	
	
	pthread_mutex_unlock(old_lock);
	printf("Group created: %s\n", groupname);
}

static void delete_group(char* groupname, struct arguments* args)
{
	if(strlen(groupname) == 0)
	{
		return;
	}
	
	struct linked_list* current = groups;
	struct linked_list* last = NULL;
	struct group* current_group = current->data;

	pthread_mutex_t* old_lock = &current->mutex;
	pthread_mutex_lock(old_lock);
	
	while(current->next != NULL)
	{
		last = current;
		current = current-> next;
		current_group = current->data;
		if(current_group != NULL && strcmp(groupname, current_group->name) == 0)
		{
			break;
		}
		pthread_mutex_lock(&current->mutex);
		pthread_mutex_unlock(old_lock);
		old_lock = &current->mutex;
	}
	
	if(current_group != NULL && strcmp(groupname, current_group->name) == 0 && args->name == current_group->master)
	{
		pthread_mutex_lock(&current->mutex);
		last->next = current->next != NULL ? current->next : NULL;
		
		pthread_mutex_destroy(&current->mutex);
		free(current_group->master);
		free(current_group->name);
		destroy_linked_list(current_group->members);
		free(current->data);
		free(current);
		printf("Group deleted: %s\n", groupname);
	}

	
	pthread_mutex_unlock(old_lock);
}
