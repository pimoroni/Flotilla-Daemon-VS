#include "main.h"

void init_client(websocketpp::connection_hdl hdl, FlotillaClient client){
	int dock_idx, channel_idx;
	for (dock_idx = 0; dock_idx < MAX_DOCKS; dock_idx++){

		if (flotilla.dock[dock_idx].state != Connected) continue;

		std::cout << "Sending Ident: " << flotilla.dock[dock_idx].ident() << std::endl;
		websocket_server.send(hdl, flotilla.dock[dock_idx].ident(), websocketpp::frame::opcode::text);

		for (channel_idx = 0; channel_idx < MAX_CHANNELS; channel_idx++){

			if (flotilla.dock[dock_idx].module[channel_idx].state != ModuleConnected) continue;

			std::string event = flotilla.dock[dock_idx].module_event(channel_idx);

			std::cout << "Client Init: " << event << std::endl;

			websocket_server.send(hdl, event, websocketpp::frame::opcode::text);
		}

	}
}


void websocket_on_message(websocketpp::connection_hdl hdl, server::message_ptr msg) {
	FlotillaClient& client = get_data_from_hdl(hdl);

	auto payload = msg->get_payload();

	if (payload.compare("hello") == 0){
		std::cout << "Client Saying Hello!" << std::endl;
		init_client(hdl, client);
		return;
	}

	if (payload.compare("ready") == 0){
		client.ready = true;
		client.connected = true;
		std::cout << "Client Ready" << std::endl;
		init_client(hdl, client);
		return;
	}

	if (payload.compare("pause") == 0){
		client.ready = false;
		std::cout << "Client Paused" << std::endl;
		return;
	}

	if (payload.at(0) == 'h' && payload.at(1) == ':'){

		int dock_index = std::stoi(payload.substr(2));

		std::string::size_type data_offset = payload.find("d:") + 2;
		std::string::size_type channel_offset;

		std::string data = payload.substr(data_offset);

		if (data.at(0) == 's'){

			data = data.substr(2); // remove "s "

			int channel_index = std::stoi(data, &channel_offset);

			data = data.substr(channel_offset + 1);

			flotilla.dock[dock_index].module[channel_index].queue_update(data);

			//printf("Queued command for dock %d, channel %d : %s\n", dock_index, channel_index, data.c_str());

			return;
		}


		if (dock_index < MAX_DOCKS){
			// All commmands addressed at a specific dock should be processed, so add them to a fifo
			flotilla.dock[dock_index].queue_command(data);
			//printf("Pushing %s to dock %i\n", strdup(msg->payload+6), dock_index);
		}

	}

	std::cout << msg->get_payload() << std::endl;
}

void websocket_on_open(websocketpp::connection_hdl hdl) {
	std::cout << "Connection Opened " << std::endl;
	//clients.insert(std::pair<websocketpp::connection_hdl,FlotillaClient>(hdl,FlotillaClient()));
	clients[hdl] = FlotillaClient();
}

void websocket_on_close(websocketpp::connection_hdl hdl) {
	std::cout << "Connection Closed " << std::endl;

	clients.erase(hdl);
}

void websocket_on_fail(websocketpp::connection_hdl hdl) {
	std::cout << "Connection Failed " << std::endl;
}