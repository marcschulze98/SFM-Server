/**
* @file
* @brief Gets messages from the connected user and either executes the
* command or forwards the message to a local user or another server
*/
#include "SFM_Server.h"

static void loop(struct string_info* info, struct arguments* args);
static void get_string_info(struct string_info* info);
static void sort_message(const struct string_info* info);
static void put_message_extern(const struct string_info* info);
static void create_group(char* groupname, struct arguments* args);
static void delete_group(char* groupname, struct arguments* args);
static void add_delete_member(char* groupname_username, struct arguments* args, bool add);
static void get_group(char* groupname, struct arguments* args);
static void get_users(struct arguments* args, bool online_only);
static bool handle_command(const struct string_info* info, struct arguments* args);

_Thread_local bool should_shutdown;

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
	//loop until we get a message, check if user still is connected
	struct return_info return_codes;
	do
	{
		return_codes = send_string(&test_connection, args->socket_fd);
		if(return_codes.error_occured)
		{
			should_shutdown = true;
			return;
		}
		sleep(1);
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
		get_string_info(info); 								//put timestamp as prefix
		if(!handle_command(info, args)) 					//check if command and handle it accordingly
		{
			sort_message(info);								//else put message in right queue
		}
	} else {
		printf("Received invalid message from: %s -> %s\n", args->name, info->message->data);
	}
}

static void get_string_info(struct string_info* info) 				//adds timestamp
{
	struct timespec ts;
	timespec_get(&ts, TIME_UTC);
	int64_t time = ts.tv_sec;
	info->timestamp = time;
	info->message_begin = 0;
		
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

	for(uint32_t i = 0; i < info->message->length; i++)
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
	uint32_t complete_length = (uint32_t)(strlen(info->source_server) + strlen(info->source_user) + strlen(info->message->data) + 8 + 3 + 1);
	struct string* message = malloc(sizeof(*message));
	char* target_server = "\0";
	struct outgoing* out;
	struct dynamic_array* queue;
	
	for(uint32_t i = 0; i < info->message->length; i++)
	{
		if(info->message->data[i] == '@')
		{
			target_server = malloc(i+1);
			strncpy(target_server, info->message->data, i);
			target_server[i] = '\0';
		}
	}
	
	bool found = false;
	pthread_mutex_lock(&outgoing_messages->mutex);
	
	for(uint32_t i = 0; i < outgoing_messages->length; i++)
	{
		out = dynamic_array_at(outgoing_messages, i);
		if(strcmp(target_server, out->target_server) == 0)
		{
			found = true;
			queue = out->messages;
			break;
		}
	}
	
	if(!found)
	{
		out = malloc(sizeof(*out));
		out->target_server = target_server;
		out->messages = new_dynamic_array();
		out->tries = 0;
		dynamic_array_push(outgoing_messages, out);
		queue = out->messages;
	} else {
		free(target_server);
	}
		
	message->data = malloc(complete_length);
	message->length = 0;
	message->capacity = complete_length;
	
	copy_helper(message, info->source_server, (uint32_t)strlen(info->source_server), '@');
	copy_helper(message, info->source_user, (uint32_t)strlen(info->source_user), ':');
	copy_helper(message, &info->timestamp, 8, '>');
	copy_helper(message, info->message->data, (uint32_t)strlen(info->message->data), '\0');
	
	dynamic_array_push(queue, message);
	
	pthread_mutex_unlock(&outgoing_messages->mutex);
}

static bool handle_command(const struct string_info* info, struct arguments* args) // check if message is a command, seperate payload from command
{
	char* command = info->message->data + info->message_begin;
	if(command[0] != '/')
		return false;
	
	struct string string_without_payload = new_string(DEFAULT_NAME_LENGTH);
	bool has_payload = false;

	for(uint32_t i = 0; i <= strlen(command); i++)
	{
		if(command[i] != ' ')
		{
			realloc_write(&string_without_payload, command[i], i);
			string_without_payload.length++;
		} else {
			has_payload = true;
			realloc_write(&string_without_payload, '\0', i);
			string_without_payload.length++;
			break;
		}
	}
	
	if(strcmp(string_without_payload.data, "/bye") == 0)
	{
		should_shutdown = true;
		printf("got /bye\n");
	} else if(strcmp(string_without_payload.data, "/creategroup") == 0 && has_payload) {
		create_group(command+strlen("/creategroup")+1, args);
	} else if(strcmp(string_without_payload.data, "/deletegroup") == 0 && has_payload) {
		delete_group(command+strlen("/creategroup")+1, args);
	} else if(strcmp(string_without_payload.data, "/addmember") == 0 && has_payload) {
		add_delete_member(command+strlen("/addmember")+1, args, true);
	} else if(strcmp(string_without_payload.data, "/deletemember") == 0 && has_payload) {
		add_delete_member(command+strlen("/deletemember")+1, args, false);
	} else if(strcmp(string_without_payload.data, "/showgroup") == 0 && has_payload) {
		get_group(command+strlen("/showgroup")+1, args);
	} else if(strcmp(string_without_payload.data, "/showgroup") == 0) {
		get_group(string_without_payload.data, args);
	} else if(strcmp(string_without_payload.data, "/onlineusers") == 0) {
		get_users(args, true);
	} else if(strcmp(string_without_payload.data, "/users") == 0) {
		get_users(args, false);
	} 
	
	free(string_without_payload.data);
	
	return true;
}

static void create_group(char* groupname, struct arguments* args) //check if group already exists, if not, create it
{
	if(strlen(groupname) == 0)
		return;
	
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

static void delete_group(char* groupname, struct arguments* args) //check if group exists and delelte it
{	
	if(strlen(groupname) == 0)
		return;
	
	pthread_mutex_lock(&groups->mutex);
	struct group* current_group;
	for(uint32_t i = 0; i < groups->length; i++)
	{
		current_group = (struct group*)dynamic_array_at(groups, i);
		if(strcmp(current_group->name, groupname) == 0 && strcmp(args->name, current_group->master) == 0)
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

static void add_delete_member(char* groupname_username, struct arguments* args, bool add) //check if group exists, then if user exists, if not, add user to group
{
	if(strlen(groupname_username) == 0)
		return;
	
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
	
	if(!group_found)
		return;

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
				if(!add)
					dynamic_array_remove(members, i);
				found = true;
				break;
			}
		}
		if(add && !found)
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

static void get_group(char* groupname, struct arguments* args)
{
	struct group* current_group;
	struct string* answer = malloc(sizeof(*answer));
	answer->data = malloc(DEFAULT_BUFFER_LENGTH);
	answer->length = 0;
	answer->capacity = DEFAULT_BUFFER_LENGTH;
	string_append(answer, "/status");
	pthread_mutex_lock(&groups->mutex);
	
	if(strcmp(groupname, "/showgroup") == 0)
	{
		string_append(answer, " groups");

		for(size_t i = 0; i < groups->length; i++)
		{
			current_group = (struct group*)dynamic_array_at(groups, i);
			if(i != 0)	
				string_append(answer, ",");
			else
				string_append(answer, " ");
			string_append(answer, current_group->name);
		}
	} else {
		
		bool found = false;
		for(size_t i = 0; i < groups->length; i++)
		{	
			current_group = (struct group*)dynamic_array_at(groups, i);
			if(strcmp(current_group->name, groupname) == 0)
			{
				found = true;
				break;
			}
		}
		if(found)
		{
			string_append(answer, " groupmembers");
			string_append(answer, " ");
			string_append(answer, groupname);

			struct dynamic_array* members = current_group->members;
			
			for(size_t i = 0; i < members->length; i++)
			{
				if(i != 0)
					string_append(answer, ",");
				else
					string_append(answer, " ");
				string_append(answer, members->data[i]);
			}
		} else {
			string_append(answer, " nogroup ");
			string_append(answer, groupname);
		}
	}
	
	adjust_string_size(answer, answer->length+1);
	realloc_write(answer, '\0', answer->length);
	answer->length++;
	
	pthread_mutex_lock(&(users+args->user_id)->messages->mutex);
	dynamic_array_push((users+args->user_id)->messages, answer);
	pthread_mutex_unlock(&(users+args->user_id)->messages->mutex);
	
	pthread_mutex_unlock(&groups->mutex);
}

static void get_users(struct arguments* args, bool online_only)
{
	struct user current_user;
	struct string* answer = malloc(sizeof(*answer));
	answer->data = malloc(DEFAULT_BUFFER_LENGTH);
	answer->length = 0;
	answer->capacity = DEFAULT_BUFFER_LENGTH;
	if(online_only)
		string_append(answer, "/status onlineusers");
	else
		string_append(answer, "/status users");
	bool first = true;

	for(size_t i = 0; i < user_count; i++)
	{
		current_user = users[i];
		if(!online_only || (online_only && (current_user.write_connected || current_user.listen_connected)))
		{
			if(!first)	
			{
				string_append(answer, ",");
			}
			else
			{
				string_append(answer, " ");
				first = false;
			}
			string_append(answer, current_user.name);
		}
	}

	pthread_mutex_lock(&(users+args->user_id)->messages->mutex);
	dynamic_array_push((users+args->user_id)->messages, answer);
	pthread_mutex_unlock(&(users+args->user_id)->messages->mutex);
}
