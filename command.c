
#include "command.h"
#include "areactor.h"
#include "network.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


void command_get_clients_number(struct client *c)
{
	int numbers = listLength(g_server.clients);
	sprintf(c->buf, "%d", listLength(g_server.clients));
	addReply(c, NULL);
}

void command_quit_client(struct client *c)
{
	freeClient(c);
}

void command_sa(struct client *c)
{
	addReply(c, "SA operate return OK");
}

void command_sb(struct client *c)
{
	addReply(c, "SB operate return OK");
}

void command_sc(struct client *c)
{
	addReply(c, "SC operate return OK");
}

struct command command_table[] = {
	{"num", command_get_clients_number},
	{"quit", command_quit_client},
	{"sa", command_sa},
	{"sb", command_sb},
	{"sc", command_sc},
};


void init_command_table()
{
	g_server.commands = command_table;
}

int get_commands_number()
{
	return sizeof(command_table)/sizeof(struct command);
}

struct command *get_command_from_name(const char *name)
{
	int i;

	for (i=0; i<sizeof(command_table)/sizeof(struct command); i++) {
		if (strcasecmp(name, command_table[i].name) == 0){
			return command_table + i;
		}
	}
	// not find name cmd
	return NULL;
}

struct command *get_command_from_index(int index)
{
	if ((index >=0) && (index < get_commands_number())){
		return command_table + index;
	}

	return NULL;
}



