#pragma once
#ifndef _FLOTILLA_DOCK_H
#define _FLOTILLA_DOCK_H

#include <mutex>
#include <string>
#include <queue>
#include <thread>

#include "Config.h"
#include "Flotilla_Module.h"

bool sp_wait_for(struct sp_port* port, std::string wait_for);
std::string sp_readline(struct sp_port* port);

enum DockState {
	Connecting,
	Connected,
	Disconnected
};

class FlotillaDock {
public:
	FlotillaDock();
	int index;
	DockState state;
	std::string name;
	std::string user;
	std::string serial;
	std::string version;
	std::string ident();
	struct sp_port* port;
	FlotillaModule module[MAX_CHANNELS];
	//std::vector<FlotillaModule> module;
	std::thread thread_dock_tick;
	std::string get_next_command(int channel);
	std::vector<std::string> get_pending_commands(void);
	std::vector<std::string> get_pending_events(void);
	bool has_pending_events();
	std::string get_next_event();
	void queue_module_event(int channel);
	std::string module_event(int channel);
	void tick();
	bool set_port(sp_port* new_port);
	void disconnect(void);
	void queue_command(std::string cmd);
	void cmd_enumerate(void);
private:
	std::mutex mutex;
	std::queue <std::string> command_queue;
	std::queue <std::string> event_queue; // Events which should be sent to clients
	bool get_version_info();
};

#endif