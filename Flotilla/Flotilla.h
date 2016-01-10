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
#pragma once
#ifndef _FLOTILLA_H_
#define _FLOTILLA_H_

#include <libserialport.h>
#include <iostream>
#include <sstream>
#include <thread>
#include <chrono>
#include <string>
#include <queue>
#include <websocketpp/common/connection_hdl.hpp>
#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/server.hpp>
#include <map>
#include <vector>

#include "Config.h"
#include "Flotilla_Client.h"
#include "Flotilla_Dock.h"

void for_each_port(void(*handle_port)(struct sp_port* port));

namespace flotilla {

	FlotillaDock dock[MAX_DOCKS];
	std::map<websocketpp::connection_hdl, FlotillaClient, std::owner_less<websocketpp::connection_hdl>> clients;
	FlotillaClient& get_client_from_hdl(websocketpp::connection_hdl hdl);
	void setup_server(int flotilla_port);
	void stop_server();
	void start_server();
	void send_to_clients(std::string command);

	websocketpp::server<websocketpp::config::asio> websocket_server;
	void init_client(websocketpp::connection_hdl hdl, FlotillaClient client);
	void websocket_on_message(websocketpp::connection_hdl hdl, websocketpp::server<websocketpp::config::asio>::message_ptr msg);
	void websocket_on_open(websocketpp::connection_hdl hdl);
	void websocket_on_close(websocketpp::connection_hdl hdl);
	void websocket_on_fail(websocketpp::connection_hdl hdl);
};

#endif
