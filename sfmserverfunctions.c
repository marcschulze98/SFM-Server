#include "SFM_Server.h"

uint32_t get_user_id(char* username)
{
	if(username)
	{
		for(uint32_t i = 1; i < user_count; i++)
		{
			if(strcmp((users+i)->name, username) == 0) return i;
		}
	}
	return 0;
}

char* contains_group(char* name)
{
	char* groupname = NULL;
	if(name)
	{
		for(uint32_t i = 0; i < strlen(name); i++)
		{
			if(name[i] == '/')
			{
				groupname = malloc(strlen(name+i+1)+1);
				strcpy(groupname, name+i+1);
				break;
			}
		}
	}

	return groupname;
}

char* get_group_user(char* groupname, uint32_t user_number)
{
	struct group* current_group;
	
	for(uint32_t i = 0; i < groups->length; i++)
	{
		current_group = (struct group*)dynamic_array_at(groups, i);
		if(strcmp(current_group->name, groupname) == 0)
		{
			return dynamic_array_at(current_group->members, user_number);
		}
	}

	return NULL;
}

void put_message_local(const struct string_info* info) //copy message in queue of the target user, replace target prefix with source prefix
{
	uint32_t server_found = 0;
	uint32_t user_id;
	char* target_name = NULL;
	char* groupname;
	
	for(uint32_t i = 0; i < info->message->length; i++)
	{
		if(server_found && (info->message->data[i] == ':'))
		{
			target_name = malloc(i-server_found+1);
			memcpy(target_name, info->message->data+server_found, i-server_found);
			target_name[i-server_found] = '\0';
			break;
		}
		if(info->message->data[i] == '@')
			server_found = i+1;
	}
	
	uint32_t complete_length = (uint32_t)(strlen(info->source_server) + strlen(info->source_user) + strlen(info->message->data + info->message_begin) + 8 + 3 + 1);
	struct string* message = malloc(sizeof(*message));
	
	message->data = malloc(complete_length);
	message->length = 0;
	message->capacity = complete_length;
	
	copy_helper(message, &info->timestamp, 8, '>');
	copy_helper(message, info->source_server, (uint32_t)strlen(info->source_server), '@');
	copy_helper(message, info->source_user, (uint32_t)strlen(info->source_user), ':');
	copy_helper(message, info->message->data + info->message_begin, (uint32_t)strlen(info->message->data + info->message_begin), '\0');
	
	pthread_mutex_lock(&groups->mutex);
	
	if((groupname = contains_group(target_name)))
	{
		char* username = NULL;
		struct string* tmp;
		for(uint32_t i = 0; (username = get_group_user(groupname, i)) != NULL; i++)
		{
			if((user_id = get_user_id(username)) == 0)
				continue;
			tmp = malloc(sizeof(*tmp));
			string_copy(tmp, message); //TODO: reference counting instead of copying every string?
			pthread_mutex_lock(&(users+user_id)->messages->mutex);
			dynamic_array_push((users+user_id)->messages, tmp);
			pthread_mutex_unlock(&(users+user_id)->messages->mutex);
		}
		free(message->data);
		free(message);
		free(groupname);
	} else {
		if((user_id = get_user_id(target_name)) == 0)
			goto cleanup;
		pthread_mutex_lock(&(users+user_id)->messages->mutex);
		dynamic_array_push((users+user_id)->messages, message);
		pthread_mutex_unlock(&(users+user_id)->messages->mutex);
	}
	
cleanup:
	free(target_name);
	pthread_mutex_unlock(&groups->mutex);
	
}

void copy_helper(struct string* message, const void* source, uint32_t length, char insert) //helper function
{
	memcpy(message->data + message->length, source, length);
	message->length += length;
	message->data[message->length] = insert;
	message->length += 1;
}
