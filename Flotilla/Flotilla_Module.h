#pragma once
#ifndef _FLOTILLA_MODULE_H
#define _FLOTILLA_MODULE_H

#include <mutex>
#include <string>
#include <queue>

#include "Config.h"

enum ModuleState {
	ModuleConnected,
	ModuleDisconnected
};

class FlotillaModule {
public:
	ModuleState state;
	void connect(std::string module_name);
	void disconnect(void);
	FlotillaModule();
	std::string name;
	char data[512];		// Store the current state of the module as an unparsed string of command data, Daemon doesn't care what it is
	void queue_command(std::string cmd);
	void queue_update(std::string cmd);
	std::string get_next_command(void);
	bool get_next_update(std::string &update);
	bool is_volatile(void);
private:
	std::string last_update;
	void empty_queue(void);
	std::mutex mutex;
	std::queue <std::string> command_queue;
	std::queue <std::string> update_queue;
};

#endif