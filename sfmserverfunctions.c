#include "SFM_Server.h"

uint32_t get_user_id(char* username)
{
	if(username)
	{
		for(uint32_t i = 1; i < user_count; i++)
		{
			if(strcmp((users+i)->name, username) == 0)
			{
				return i;
			}
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
	struct dynamic_array* members;
	
	for(uint32_t i = 0; i < groups->length; i++)
	{
		current_group = (struct group*)dynamic_array_at(groups, i);
		if(strcmp(current_group->name, groupname) == 0)
		{
			members = current_group->members;
			return dynamic_array_at(members, user_number);
		}
	}

	return NULL;
}
