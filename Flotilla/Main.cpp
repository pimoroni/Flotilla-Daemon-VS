#include "main.h"

void cleanup(void){
	int i;
	running = 0;

	thread_dock_scan.join();
	thread_update_clients.join();
	thread_update_docks.join();

	for (i = 0; i < MAX_DOCKS; i++){
		//flotilla.dock[i].thread_dock_tick.join();
		std::cout << "Disconnecting Dock " << i << std::endl;
		flotilla.dock[i].disconnect();
		//std::cout << "Stopping Dock " << i << std::endl;
		//flotilla.dock[i].stop();
		std::cout << "Done... " << i << std::endl;
	}
}

void sigint_handler(int sig_num){
	//fprintf(stdout, "Exiting cleanly, please wait...");
	std::cout << "Exiting cleanly, please wait..." << std::endl;
	cleanup();
	exit(1);
}

void update_connected_docks(){
	enum sp_return result;
	struct sp_port** ports = NULL;
	result = sp_list_ports(&ports);
	if (result != SP_OK){ return; };

	int x, y;
	for (x = 0; x < MAX_DOCKS; x++){
		if (flotilla.dock[x].port == NULL || flotilla.dock[x].state == Disconnected) {
			//printf("Port is null? %d\n", x);
			continue;
		};

		bool found = FALSE;
		y = 0;

		while (ports[y] != NULL){
			struct sp_port* port = ports[y];

			const char* a = sp_get_port_name(flotilla.dock[x].port);
			const char* b = sp_get_port_name(port);

			if (strcmp(a, b) == 0){
				found = true;
			}

			y++;
		}

		if (!found){
			flotilla.dock[x].disconnect();
			//printf("Dock Lost\n");
			std::cout << "Dock Lost" << std::endl;
		}
	}

	y = 0;
	while (ports[y] != NULL){
		scan_for_host(ports[y]);
		y++;
	}

	sp_free_port_list(ports);
}

/* Scan for new docks */
void worker_dock_scan(void){
	while (running){
		update_connected_docks();
		std::this_thread::sleep_for(std::chrono::seconds(1));
	}

	return;
}

#define PID 9220
#define VID 1003

void scan_for_host(struct sp_port* port){
	int x;

	const char* port_name = sp_get_port_name(port);
	const char* port_desc = sp_get_port_description(port);
	int usb_vid, usb_pid;

	sp_get_port_usb_vid_pid(port, &usb_vid, &usb_pid);

	for (x = 0; x < MAX_DOCKS; x++){
		if (flotilla.dock[x].state != Disconnected && strcmp(port_name, sp_get_port_name(flotilla.dock[x].port)) == 0){
			return;
		}
	}

	//std::cout << "Found port: " << port_desc << ", Name: " << port_name << std::endl;
	//std::cout << "PID: " << usb_pid << " VID: " << usb_vid << std::endl;

	if (strcmp(port_desc, "Flotilla Dock") == 0 || (usb_vid == VID && usb_pid == PID)){

		//printf("Potential host found: %s\n", port_name);

		FlotillaDock temp;

		if (temp.set_port(port)){
			temp.disconnect();

			//printf("Successfully Identified Dock. Serial: %s\n", temp.serial.c_str());
			std::cout << "Successfully Identified Dock. Serial: " << temp.serial << std::endl;

			/*
			printf("Host Version Info: %s\n", port_name);
			printf("Host Version: %s\n", temp_version);
			printf("Host Serial: %s\n", temp_serial);
			printf("Host User: %s\n", temp_user);
			printf("Host Name: %s\n", temp_name);
			*/

			for (x = 0; x < MAX_DOCKS; x++){
				if (flotilla.dock[x].serial.compare(temp.serial) == 0){

					//printf("Found Existing Dock With Serial %s at index %d\n", temp.serial.c_str(), x);
					std::cout << "Found existing Dock with serial " << temp.serial << " at index " << x << std::endl;

					if (flotilla.dock[x].state == Disconnected){

						if (flotilla.dock[x].set_port(port)){
							//printf("Success! %d\n", x);
							std::cout << "Success! " << x << std::endl;
							//flotilla.dock[x].start();
						};

					}

					return;
				}
			}

			for (x = 0; x < MAX_DOCKS; x++){
				if (flotilla.dock[x].state == Disconnected){
					//printf("Using Dock slot at index %d\n", x);
					std::cout << "Using Dock slot at index " << x << std::endl;

					if (flotilla.dock[x].set_port(port)){
						//printf("Success! %d\n", x);
						std::cout << "Success! " << x << std::endl;
						//flotilla.dock[x].start();
					};

					return;
				}
			}

		}

	}
}

void for_each_port(void(*handle_port)(struct sp_port* port)){
	enum sp_return res;
	struct sp_port** ports = NULL;
	res = sp_list_ports(&ports);
	if (res != SP_OK){
		return;
	}
	int x = 0;
	while (ports[x] != NULL){
		(*handle_port)(ports[x]);
		x++;
	}
	sp_free_port_list(ports);
}

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

void send_to_clients(std::string command){

	//std::cout << "Sending to clients: " << command << std::endl;

	for (auto client : clients){

		if (!client.second.ready) continue;

		//std::cout << "Sending..." << std::endl;
		websocket_server.send(client.first, command, websocketpp::frame::opcode::text);

	}
}



/* Update Threads */

void worker_update_clients(void){
	while (running){

		auto start = std::chrono::high_resolution_clock::now();

		int dock_idx; // , channel_idx;

		for (dock_idx = 0; dock_idx < MAX_DOCKS; dock_idx++){

			while (flotilla.dock[dock_idx].has_pending_events()){

				send_to_clients(flotilla.dock[dock_idx].get_next_event());

			};

			if (flotilla.dock[dock_idx].state != Connected) continue;

			//std::cout << "Dock Update: " << flotilla.dock[dock_idx].serial << std::endl;

			for (auto command : flotilla.dock[dock_idx].get_pending_commands()){
				send_to_clients(command);
			}

			/*for (channel_idx = 0; channel_idx < MAX_CHANNELS; channel_idx++){

				if (flotilla.dock[dock_idx].module[channel_idx].state != ModuleConnected) continue;

				std::string command = flotilla.dock[dock_idx].get_next_command(channel_idx);

				if (!command.empty()) send_to_clients(command);

			}*/

		}

		send_to_clients("update");

		auto elapsed = std::chrono::high_resolution_clock::now() - start;

		long long microseconds = std::chrono::duration_cast<std::chrono::microseconds>(elapsed).count();

		std::this_thread::sleep_for(std::chrono::microseconds(100000 - microseconds));
	}

	return;

}

void worker_update_docks(void){
	while (running){
		// Iterate through every dock and pass messages on to the client

		auto start = std::chrono::high_resolution_clock::now();

		int dock_idx;
		for (dock_idx = 0; dock_idx < MAX_DOCKS; dock_idx++){
			if (flotilla.dock[dock_idx].state != Connected) continue;

			flotilla.dock[dock_idx].tick();
		}

		auto elapsed = std::chrono::high_resolution_clock::now() - start;

		long long microseconds = std::chrono::duration_cast<std::chrono::microseconds>(elapsed).count();

		//std::this_thread::sleep_for(std::chrono::microseconds(100000 - microseconds));
		std::this_thread::sleep_for(std::chrono::microseconds(1000 - microseconds));
	}
}

/* Web Sockets */

FlotillaClient& get_data_from_hdl(websocketpp::connection_hdl hdl){
	auto client = clients.find(hdl);

	if (client == clients.end()){
		throw std::invalid_argument("Could not locate connection data!");
	}

	return client->second;
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

			//std::cout << "Queued command for dock " << dock_index << ", channel " << channel_index << ": " << data << std::endl;

			return;
		}


		if (dock_index < MAX_DOCKS){
			// All commmands addressed at a specific dock should be processed, so add them to a fifo
			flotilla.dock[dock_index].queue_command(data);
			
			//std::cout << "Pushing " << data  << " to dock " << dock_index << std::endl;
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

/* Main */

int main(int argc, char *argv[])
{
	int i;
	running = 1;

	signal(SIGINT, sigint_handler);

	thread_dock_scan = std::thread(worker_dock_scan);
	thread_update_clients = std::thread(worker_update_clients);
	thread_update_docks = std::thread(worker_update_docks);

	for (i = 0; i < MAX_DOCKS; i++){
		flotilla.dock[i].index = i;
	}

	websocket_server.clear_access_channels(websocketpp::log::alevel::all); // Turn off all console output
	websocket_server.set_message_handler(&websocket_on_message);
	websocket_server.set_fail_handler(&websocket_on_fail);
	websocket_server.set_open_handler(&websocket_on_open);
	websocket_server.set_close_handler(&websocket_on_close);

	std::cout << "Flotilla Ready To Set Sail..." << std::endl;

	websocket_server.init_asio();
	websocket_server.set_reuse_addr(TRUE);
	websocket_server.listen(boost::asio::ip::tcp::v4(), FLOTILLA_PORT);

	std::cout << "Listening on port " << FLOTILLA_PORT << std::endl;

	websocket_server.start_accept();
	websocket_server.run();

	cleanup();

	return 0;

}
