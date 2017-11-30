#include "SFM_Server.h"

static void loop(struct string_info* info, struct arguments* args);
static void get_string_info(struct string_info* info);
static void sort_message(const struct string_info* info);
static void put_message_extern(const struct string_info* info);
static void put_message_local(const struct string_info* info);
static void create_group(char* groupname, struct arguments* args);
static void delete_group(char* groupname, struct arguments* args);
static void add_group(char* groupname_username, struct arguments* args);
static void copy_helper(struct string* message, const void* source, uint32_t length, char insert);
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
		
	for(uint32_t i = 0; i < info->message->length; i++)
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
	


	uint32_t complete_length = (uint32_t)(strlen(info->source_server) + strlen(info->source_user) + strlen(info->message->data + info->message_begin) + 8 + 3 + 1);
	struct string* message = malloc(sizeof(struct string));
	
	message->data = malloc(complete_length);
	message->length = 0;
	message->capacity = complete_length;
	
	copy_helper(message, (const void*)&info->timestamp, 8, '>');
	copy_helper(message, (const void*)info->source_server, (uint32_t)strlen(info->source_server), '@');
	copy_helper(message, (const void*)info->source_user, (uint32_t)strlen(info->source_user), ':');
	copy_helper(message, (const void*)(info->message->data + info->message_begin), (uint32_t)strlen(info->message->data + info->message_begin), '\0');
	
	pthread_mutex_lock(&(users+user_id)->messages->mutex);
	dynamic_array_push((users+user_id)->messages, message);
	pthread_mutex_unlock(&(users+user_id)->messages->mutex);
}

static void copy_helper(struct string* message, const void* source, uint32_t length, char insert)
{
	memcpy(message->data + message->length, source, length);
	message->length += length;
	message->data[message->length] = insert;
	message->length += 1;
}

static bool handle_command(const struct string_info* info, struct arguments* args)
{
	char* command = info->message->data + info->message_begin;
	if(command[0] != '/')
	{
		return false;
	}
	
	struct string string_without_payload = new_string(DEFAULT_NAME_LENGTH);
	bool has_payload = false;

	for(uint32_t i = 0; i <= strlen(command); i++)
	{
		if(command[i] != ' ')
		{
			realloc_write(&string_without_payload, command[i], i);
		} else {
			has_payload = true;
			realloc_write(&string_without_payload, '\0', i);
			break;
		}
	}
	
	if(strcmp(string_without_payload.data, "/bye") == 0)
	{
		should_shutdown = true;
		printf("got /bye\n");
	} else if(strcmp(string_without_payload.data, "/creategroup") == 0 && has_payload) {
		create_group(command+13, args);
	} else if(strcmp(string_without_payload.data, "/deletegroup") == 0 && has_payload) {
		delete_group(command+13, args);
	} else if(strcmp(string_without_payload.data, "/addgroup") == 0 && has_payload) {
		add_group(command+10, args);
	}
	
	free(string_without_payload.data);
	
	return true;
}

static void create_group(char* groupname, struct arguments* args)
{
	if(strlen(groupname) == 0)
	{
		return;
	}
	
	pthread_mutex_lock(&groups->mutex);

	struct group* current_group;
	
	for(uint32_t i = 0; i < groups->length; i++)
	{
		current_group = (struct group*)dynamic_array_at(groups, i);
		if(strcmp(current_group->name, groupname) == 0)
		{
			pthread_mutex_unlock(&groups->mutex);
			return;
		}
	}
	
	struct group* new_group = malloc(sizeof(*new_group));
	new_group->master = malloc(strlen(args->name)+1);
	strcpy(new_group->master, args->name);
	new_group->name = malloc(strlen(groupname)+1);
	strcpy(new_group->name, groupname);
	new_group->members = new_dynamic_array();
	dynamic_array_push(groups, new_group);
	
	pthread_mutex_unlock(&groups->mutex);
	printf("Group created: %s\n", groupname);
}

static void delete_group(char* groupname, struct arguments* args)
{	
	if(strlen(groupname) == 0)
	{
		return;
	}
	
	pthread_mutex_lock(&groups->mutex);
	struct group* current_group;
	for(uint32_t i = 0; i < groups->length; i++)
	{
		current_group = (struct group*)dynamic_array_at(groups, i);
		if(strcmp(current_group->name, groupname) == 0)
		{
			free(current_group->master);
			free(current_group->name);
			destroy_dynamic_array(current_group->members);
			dynamic_array_remove(groups, i);
			printf("Group deleted: %s\n", groupname);
			break;
		}
	}
	
	pthread_mutex_unlock(&groups->mutex);
}

static void add_group(char* groupname_username, struct arguments* args)
{
	if(strlen(groupname_username) == 0)
	{
		return;
	}
	
	struct string groupname = new_string(DEFAULT_NAME_LENGTH);
	struct string username = new_string(DEFAULT_NAME_LENGTH);
	uint32_t group_found = 0;
	
	for(uint32_t i = 0; i <= strlen(groupname_username); i++)
	{
		if(groupname_username[i] == ' ')
		{
			realloc_write(&groupname, '\0', i);
			groupname.length++;
			group_found = ++i;
		}
		
		if(!group_found)
		{
			realloc_write(&groupname, groupname_username[i], i);
			groupname.length++;
		} else {
			realloc_write(&username, groupname_username[i], i-group_found);
			username.length++;
		}
	}
	printf("%s %s\n", groupname.data, username.data);
	
	
	pthread_mutex_lock(&groups->mutex);
	
	struct group* current_group;
	bool found = false;
	for(uint32_t i = 0; i < groups->length; i++)
	{
		current_group = (struct group*)dynamic_array_at(groups, i);
		if(strcmp(current_group->name, groupname.data) == 0)
		{
			found = true;
			break;
		}
	}
	
	if(found && strcmp(args->name, current_group->master) == 0)
	{
		found = false;
		struct dynamic_array* members = current_group->members;
		
		for(uint32_t i = 0; i < members->length; i++)
		{
			if(strcmp((char*)dynamic_array_at(members, i), username.data) == 0)
			{
				found = true;
				break;
			}
		}
		if(!found)
		{
			char* tmp = malloc(username.length);
			strcpy(tmp, username.data);
			dynamic_array_push(members, tmp);
			printf("added member to %s: %s\n", groupname.data, username.data);
		}
	}
	
	pthread_mutex_unlock(&groups->mutex);
	free(groupname.data);
	free(username.data);
}
