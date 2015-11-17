/*


From Dock to Client ( via Daemon ):
s 1/dial 412			- New update for module dial on channel 1
s 3/colour 215,215,215	- New update for module colour on channel 3
c 1/dial 				- Dial connected to channel 1
d 1/dial				- Dial disconnected from channel 1

From Client to Dock ( via Daemon ):
s 1 123					- Update module on channel 1 with data - we don't need to say the module name
s 3 123,456				- Update module on channel 3 with data
v 						- Get version information, will return 5 lines:
# Flotilla ready to set sail..				- The banner
# Version: 0.1 								- The version number, minor and major
# Serial: 1234567781987564738972			- 22 character serial, hexadecimal repr of 11 bytes
# User: username							- User name saved to the dock EEPROM
# Dock: dockname							- Dock name saved to the dock EEPROM
p 0 					- Turn life-Ring power off
p 1 					- Turn life-ring power on
n 						- Request user info, will return as two lines:
# User: username
# Dock: dockname
n u username			- Update the saved username
n d dockname			- Update the saved dock name


From Client to Daemon: ( These may change )

Handshake:
list 					- Client is asking for a list of connected docks
returns one line for each connected dock, like so:
# Host: version,serial,user,name

ready					- Client sends ready when it's connected and wishes to receive updates
This will probably change to some method of subscribing to a particular dock

*/

#include <libserialport.h>
#include <iostream>
#include <sstream>
#include <thread>
#include <mutex>
#include <chrono>
#include <string>
#include <queue>
#include <set>
#include <websocketpp/common/connection_hdl.hpp>

#ifndef _FLOTILLA_H_
#define _FLOTILLA_H_

#define MAX_DOCKS    8
#define MAX_CHANNELS 8

#define BAUD_RATE    9600

#ifndef TRUE
#define TRUE  (1==1)
#endif
#ifndef FALSE
#define FALSE (1==0)
#endif

bool sp_wait_for(struct sp_port* port, std::string wait_for);
std::string sp_readline(struct sp_port* port);
void for_each_port(void(*handle_port)(struct sp_port* port));

int main(int argc, char *argv[]);

class FlotillaClient {
public:
	bool connected;	// Flag to indicate client has connected, will usually be true except in odd circumstances, possibly unecessary
	bool ready;     // Flag to indicate client ready status, module updates should not be sent unless client is ready
	void queue_command(std::string cmd);
	FlotillaClient();
	FlotillaClient(const FlotillaClient& src) :
		connected(src.connected),
		ready(src.ready),
		command_queue(src.command_queue),
		mutex() {};
	FlotillaClient& operator=(FlotillaClient src) {
		std::swap(connected, src.connected);
		std::swap(ready, src.ready);
		std::swap(command_queue, src.command_queue);
		return *this;
	}
private:
	void empty_queue(void);
	std::mutex mutex;
	std::queue <std::string> command_queue;
};

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

enum DockState {
	Connecting,
	Connected,
	Disconnected
};

class FlotillaDock {
public:
	FlotillaDock();
	~FlotillaDock();
	int index;
	DockState state = Disconnected;
	std::string name = "";
	std::string user = "";
	std::string serial = "";
	std::string version = "";
	std::string ident();
	struct sp_port* port;
	FlotillaModule module[MAX_CHANNELS];
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

class Flotilla {
public:
	Flotilla();
	FlotillaDock dock[MAX_DOCKS];
private:
};

#endif
