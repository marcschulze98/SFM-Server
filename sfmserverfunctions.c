#include "SFM_Server.h"

uint32_t get_user_id(char* username)
{
	if(username == NULL)
	{
		return 0;
	}
	
	for(uint32_t i = 1; i < user_count; i++)
	{
		if(strcmp((users+i)->name, username) == 0)
		{
			return i;
			break;
		}
	}
	
	return 0;
}

struct linked_list_return find_groupname(struct linked_list* current, struct string* target)
{
	struct linked_list_return returning = { .last = NULL, .current = current, .found = false};
	struct group* current_group = current->data;
	
	pthread_mutex_lock(&current->mutex);
	
	while(current->next != NULL)
	{
		if(returning.last != NULL)
		{
			pthread_mutex_unlock(&returning.last->mutex);
		}
		returning.last = current;
		current = current-> next;
		current_group = current->data;	
		pthread_mutex_lock(&current->mutex);
		if(current_group != NULL && strcmp(target->data, current_group->name) == 0)
		{
			returning.found = true;
			break;
		}
	}
	
	if(current_group != NULL && strcmp(target->data, current_group->name) == 0)
	{
		returning.found = true;
	}
	
	returning.current = current;
	return returning;
}
