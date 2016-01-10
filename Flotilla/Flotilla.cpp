#include "Flotilla.h"
#include "Timestamp.h"

using websocketpp::lib::bind;
using websocketpp::lib::placeholders::_1;
using websocketpp::lib::placeholders::_2;

Flotilla::Flotilla() {
	for (auto i = 0; i < MAX_DOCKS; i++) {
		dock[i].index = i;
	}
}

void Flotilla::update_clients() {
	for (auto dock_idx = 0; dock_idx < MAX_DOCKS; dock_idx++) {

		for (auto event : dock[dock_idx].get_pending_events()) {
			send_to_clients(event);
		}

		if (dock[dock_idx].state != Connected) continue;

		//std::cout << GetTimestamp() << "Dock Update: " << flotilla.dock[dock_idx].serial << std::endl;

		for (auto command : dock[dock_idx].get_pending_commands()) {
			send_to_clients(command);
		}

	}
}

void Flotilla::update_docks() {
	for (auto dock_idx = 0; dock_idx < MAX_DOCKS; dock_idx++) {
		if (dock[dock_idx].state != Connected) continue;
		dock[dock_idx].tick();
	}
}

FlotillaClient& Flotilla::get_client_from_hdl(websocketpp::connection_hdl hdl) {
	auto client = clients.find(hdl);

	if (client == clients.end()) {
		throw std::invalid_argument("Could not locate connection data!");
	}

	return client->second;
}

void Flotilla::send_to_clients(std::string command) {

	if (websocket_server.stopped()) return;

	//std::cout << GetTimestamp() << "Sending to clients: " << command << std::endl;

	for (auto client : clients) {

		if (!client.second.ready) continue;

		//std::cout << GetTimestamp() << "Sending..." << std::endl;
		try {
			websocket_server.send(client.first, command, websocketpp::frame::opcode::text);
		}
		catch (websocketpp::exception e) {
			std::cout << GetTimestamp() << "Main.cpp: lib:error_code" << e.m_msg << std::endl;
		}

	}
}

/* Web Sockets */
void Flotilla::start_server(void) {
	websocket_server.start_accept();
	websocket_server.run();
}

void Flotilla::setup_server(int flotilla_port) {

	websocket_server.clear_access_channels(websocketpp::log::alevel::all); // Turn off all console output
	websocket_server.set_message_handler(bind(&Flotilla::websocket_on_message, this, ::_1, ::_2));
	websocket_server.set_fail_handler(bind(&Flotilla::websocket_on_fail, this, ::_1));
	websocket_server.set_open_handler(bind(&Flotilla::websocket_on_open, this, ::_1));
	websocket_server.set_close_handler(bind(&Flotilla::websocket_on_close, this, ::_1));

	websocket_server.init_asio();
	websocket_server.set_reuse_addr(TRUE);
	websocket_server.listen(boost::asio::ip::tcp::v4(), flotilla_port);
}

void Flotilla::stop_server(void) {
	std::cout << GetTimestamp() << "Stopping WebSocket server..." << std::endl;
	websocket_server.stop_listening();

	for (auto client : clients) {

		websocket_server.send(client.first, "bye", websocketpp::frame::opcode::text);
		try {
			websocket_server.close(client.first, websocketpp::close::status::going_away, "Flotilla Server Shutting Down");
		}
		catch (websocketpp::lib::error_code error_code) {
			std::cout << GetTimestamp() << "Main.cpp: lib:error_code" << error_code << std::endl;
		}
	}
}

void Flotilla::websocket_on_message(websocketpp::connection_hdl hdl, websocketpp::server<websocketpp::config::asio>::message_ptr msg) {
	FlotillaClient& client = get_client_from_hdl(hdl);

	auto payload = msg->get_payload();

	if (payload.compare("quit") == 0) {
		std::cout << GetTimestamp() << "Quit Received" << std::endl;
		stop_server();
		return;
	}

	if (payload.compare("hello") == 0) {
		std::cout << GetTimestamp() << "Client Saying Hello!" << std::endl;
		init_client(hdl, client);
		return;
	}

	if (payload.compare("ready") == 0) {
		client.ready = true;
		client.connected = true;
		std::cout << GetTimestamp() << "Client Ready" << std::endl;
		init_client(hdl, client);
		return;
	}

	if (payload.compare("pause") == 0) {
		client.ready = false;
		std::cout << GetTimestamp() << "Client Paused" << std::endl;
		return;
	}

	if (payload.at(0) == 'h' && payload.at(1) == ':') {

		int dock_index = std::stoi(payload.substr(2));

		std::string::size_type data_offset = payload.find("d:") + 2;
		std::string::size_type channel_offset;

		std::string data = payload.substr(data_offset);

		if (data.at(0) == 's') {

			data = data.substr(2); // remove "s "

			int channel_index = std::stoi(data, &channel_offset) - 1;

			data = data.substr(channel_offset + 1);

			dock[dock_index].module[channel_index].queue_update(data);

			//std::cout << GetTimestamp() << "Queued command for dock " << dock_index << ", channel " << channel_index << ": " << data << std::endl;

			return;
		}


		if (dock_index < MAX_DOCKS) {
			// All commmands addressed at a specific dock should be processed, so add them to a fifo
			dock[dock_index].queue_command(data);

			//std::cout << GetTimestamp() << "Pushing " << data  << " to dock " << dock_index << std::endl;
		}

	}

	std::cout << GetTimestamp() << msg->get_payload() << std::endl;
}

void Flotilla::websocket_on_open(websocketpp::connection_hdl hdl) {
	std::cout << GetTimestamp() << "Connection Opened" << std::endl;
	//clients.insert(std::pair<websocketpp::connection_hdl,FlotillaClient>(hdl,FlotillaClient()));
	clients[hdl] = FlotillaClient();
}

void Flotilla::websocket_on_close(websocketpp::connection_hdl hdl) {
	std::cout << GetTimestamp() << "Connection Closed" << std::endl;

	clients.erase(hdl);
}

void Flotilla::websocket_on_fail(websocketpp::connection_hdl hdl) {
	std::cout << GetTimestamp() << "Connection Failed" << std::endl;

	clients.erase(hdl);
}

void Flotilla::init_client(websocketpp::connection_hdl hdl, FlotillaClient client) {
	int dock_idx, channel_idx;
	for (dock_idx = 0; dock_idx < MAX_DOCKS; dock_idx++) {

		if (dock[dock_idx].state != Connected) continue;

		std::cout << GetTimestamp() << "Sending Ident: " << dock[dock_idx].ident() << std::endl;
		websocket_server.send(hdl, dock[dock_idx].ident(), websocketpp::frame::opcode::text);

		for (channel_idx = 0; channel_idx < MAX_CHANNELS; channel_idx++) {

			if (dock[dock_idx].module[channel_idx].state != ModuleConnected) continue;

			std::string event = dock[dock_idx].module_event(channel_idx);

			//std::cout << GetTimestamp() << "Main.cpp: Client Init: " << event << std::endl;

			websocket_server.send(hdl, event, websocketpp::frame::opcode::text);
		}

	}
}

void for_each_port(void(*handle_port)(struct sp_port* port)) {
	enum sp_return res;
	struct sp_port** ports = NULL;
	res = sp_list_ports(&ports);
	if (res != SP_OK) {
		return;
	}
	int x = 0;
	while (ports[x] != NULL) {
		(*handle_port)(ports[x]);
		x++;
	}
	sp_free_port_list(ports);
}
