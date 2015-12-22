#include "Main.h"

void cleanup(void){
	int i;
	running = 0;

	thread_dock_scan.join();
	thread_update_clients.join();
	thread_update_docks.join();

	for (i = 0; i < MAX_DOCKS; i++){
		//flotilla.dock[i].thread_dock_tick.join();
		std::cout << "Disconnecting Dock " << (i+1) << std::endl;
		flotilla.dock[i].disconnect();
		//std::cout << "Stopping Dock " << i << std::endl;
		//flotilla.dock[i].stop();
		//std::cout << "Done... " << std::endl;
	}

#if defined(__linux__) || defined(__APPLE__)
	if( should_daemonize ){
		std::remove(pid_file_path.c_str());
	}
#endif
}

void websocket_stop(void){
	std::cout << "Stopping WebSocket server..." << std::endl;
	websocket_server.stop_listening();

	for (auto client : clients){

		websocket_server.send(client.first, "bye", websocketpp::frame::opcode::text);
		try{
			websocket_server.close(client.first, websocketpp::close::status::going_away, "Flotilla Server Shutting Down");
		}catch(websocketpp::lib::error_code error_code){
			std::cout << "Main.cpp: lib:error_code" << error_code << std::endl;
		}
	}
}

void sigint_handler(int sig_num){
	//std::cout << "Exiting cleanly, please wait..." << std::endl;
	websocket_stop();
	//exit(1);
}

void update_connected_docks() {
	enum sp_return result;
	struct sp_port** ports = NULL;
	result = sp_list_ports(&ports);
	if (result != SP_OK) { return; };

	int x = 0;
	int y = 0;

	/*while (ports[y] != NULL) {
		struct sp_port* port = ports[y];
		const char * name = sp_get_port_name(port);
		const char * desc = sp_get_port_name(port);
		int usb_vid, usb_pid;

		sp_get_port_usb_vid_pid(port, &usb_vid, &usb_pid);

		std::cout << "Checking serial port " << name << " " << desc << " " << usb_vid << ":" << usb_pid << std::endl;

		y++;
	}*/

	for (x = 0; x < MAX_DOCKS; x++) {

		if (flotilla.dock[x].port == NULL || flotilla.dock[x].state == Disconnected) {
			continue;
		};

		//std::cout << "Checking for dock " << flotilla.dock[x].name << std::endl;

		const char* a = sp_get_port_name(flotilla.dock[x].port);
		bool found = false;

		while (ports[y] != NULL) {
			struct sp_port* port = ports[y];
			const char * b = sp_get_port_name(port);
			int usb_vid, usb_pid;

			sp_get_port_usb_vid_pid(port, &usb_vid, &usb_pid);

			//std::cout << "Checking serial port " << b << std::endl;

			if (b != NULL && usb_vid == VID && usb_pid == PID && strcmp(a, b) == 0) {
				//std::cout << "Found dock " << flotilla.dock[x].name << std::endl;
				found = true;
			}

			y++;
		}

		if (!found) {
			std::cout << "Dock " << (x+1) << " Disconnected" << std::endl;
			flotilla.dock[x].disconnect();
		}
	}

	y = 0;
	while (ports[y] != NULL) {
		scan_for_host(ports[y]);
		y++;
	}

	sp_free_port_list(ports);
}

/* Scan for new docks */
void worker_dock_scan(void) {
	while (running) {
		update_connected_docks();
		std::this_thread::sleep_for(std::chrono::seconds(1));
	}

	return;
}

//#define DEBUG_SCAN_FOR_HOST

void scan_for_host(struct sp_port* port) {
	int x;

	const char* port_name = sp_get_port_name(port);
	const char* port_desc = sp_get_port_description(port);
	int usb_vid, usb_pid;

	if (port_name == NULL || port_desc == NULL) {
		return;
	}

#ifdef DEBUG_SCAN_FOR_HOST
	std::cout << "Main.cpp: Checking port: " << port_name << " : " << port_desc << std::endl;
#endif

	if(sp_get_port_usb_vid_pid(port, &usb_vid, &usb_pid) != SP_OK){
#ifdef DEBUG_SCAN_FOR_HOST
        std::cout << "Main.cpp: Could not get VID/PID" << std::endl;
#endif
        return;
    }
	
    if (usb_vid != VID || usb_pid != PID) {
		return;
	}

#ifdef DEBUG_SCAN_FOR_HOST
    std::cout << "Main.cpp: Got details VID: " << usb_vid << " PID: " << usb_pid << std::endl;
#endif

	for (x = 0; x < MAX_DOCKS; x++) {
		if (flotilla.dock[x].state != Disconnected) {
			const char* existing_port_name = sp_get_port_name(flotilla.dock[x].port);
			if (strcmp(port_name, existing_port_name) == 0) {
#ifdef DEBUG_SCAN_FOR_HOST
				std::cout << "Main.cpp: Found existing dock entry for " << port_name << std::endl;
#endif
				return;
			}
		}
	}

	//std::cout << "Found port: " << port_desc << ", Name: " << port_name << std::endl;
	//std::cout << "PID: " << usb_pid << " VID: " << usb_vid << std::endl;


	FlotillaDock temp;

	if (temp.set_port(port)){
		temp.disconnect();
#ifdef DEBUG_SCAN_FOR_HOST
		std::cout << "Main.cpp: Successfully Identified Dock. Serial: " << temp.serial << std::endl;
#endif
		/*
		printf("Host Version Info: %s\n", port_name);
		printf("Host Version: %s\n", temp_version);
		printf("Host Serial: %s\n", temp_serial);
		printf("Host User: %s\n", temp_user);
		printf("Host Name: %s\n", temp_name);
		*/

		/*for (x = 0; x < MAX_DOCKS; x++){
			if (flotilla.dock[x].serial.compare(temp.serial) == 0){

				std::cout << "Found existing Dock with serial " << temp.serial << " at index " << x << std::endl;

				if (flotilla.dock[x].state == Disconnected){

					if (flotilla.dock[x].set_port(port)){
						std::cout << "Success! " << x << std::endl;
						//flotilla.dock[x].start();
					};

				}

				return;
			}
		}*/

		for (x = 0; x < MAX_DOCKS; x++){
			if (flotilla.dock[x].state == Disconnected){
#ifdef DEBUG_SCAN_FOR_HOST
				std::cout << "Main.cpp: Using Dock slot at index " << x << std::endl;
#endif
				if (flotilla.dock[x].set_port(port)){
#ifdef DEBUG_SCAN_FOR_HOST
					std::cout << "Main.cpp: Success! " << x << std::endl;
#endif
					std::cout << "Dock " << (x+1) << " Connected!" << std::endl;
					flotilla.dock[x].cmd_enumerate();
				};

				return;
			}
		}

	}

}

void init_client(websocketpp::connection_hdl hdl, FlotillaClient client){
	int dock_idx, channel_idx;
	for (dock_idx = 0; dock_idx < MAX_DOCKS; dock_idx++){

		if (flotilla.dock[dock_idx].state != Connected) continue;

		std::cout << "Main.cpp: Sending Ident: " << flotilla.dock[dock_idx].ident() << std::endl;
		websocket_server.send(hdl, flotilla.dock[dock_idx].ident(), websocketpp::frame::opcode::text);

		for (channel_idx = 0; channel_idx < MAX_CHANNELS; channel_idx++){

			if (flotilla.dock[dock_idx].module[channel_idx].state != ModuleConnected) continue;

			std::string event = flotilla.dock[dock_idx].module_event(channel_idx);

			std::cout << "Main.cpp: Client Init: " << event << std::endl;

			websocket_server.send(hdl, event, websocketpp::frame::opcode::text);
		}

	}
}

void send_to_clients(std::string command){

	if (websocket_server.stopped()) return;

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

			for (auto event : flotilla.dock[dock_idx].get_pending_events()){
				send_to_clients(event);
			}

			if (flotilla.dock[dock_idx].state != Connected) continue;

			//std::cout << "Dock Update: " << flotilla.dock[dock_idx].serial << std::endl;

			for (auto command : flotilla.dock[dock_idx].get_pending_commands()){
				send_to_clients(command);
			}

		}

		//send_to_clients("update");

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

	if (payload.compare("quit") == 0){
		std::cout << "Quit Received" << std::endl;
		websocket_stop();
		return;
	}

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
	std::cout << "Connection Opened" << std::endl;
	//clients.insert(std::pair<websocketpp::connection_hdl,FlotillaClient>(hdl,FlotillaClient()));
	clients[hdl] = FlotillaClient();
}

void websocket_on_close(websocketpp::connection_hdl hdl) {
	std::cout << "Connection Closed" << std::endl;

	clients.erase(hdl);
}

void websocket_on_fail(websocketpp::connection_hdl hdl) {
	std::cout << "Connection Failed" << std::endl;

	clients.erase(hdl);
}

/*
bool CtrlHandler(DWORD fdwCtrlType){
	switch (fdwCtrlType){
	case CTRL_C_EVENT:
	case CTRL_CLOSE_EVENT:
		websocket_stop();
		std::cout << "Doing closey things..." << std::endl;
		std::cout << "Does this stay open?..." << std::endl;
		while (!safe_to_exit){
			std::cout << "Waiting..." << std::endl;
			std::this_thread::sleep_for(std::chrono::microseconds(1000000));
		};
		return TRUE;
	default:
		return FALSE;
	}
}
*/

int daemonize(){
    if (pid_t pid = fork())
    {
		if (pid > 0)
		{
			// We're in the parent process and need to exit.
			//
			// should also precede the second fork().
			exit(0);
		}
		else
		{
			syslog(LOG_ERR | LOG_USER, "First fork failed: %m");
			exit(1);
		}
    }

    // Make the process a new session leader. This detaches it from the
    // terminal.
    setsid();

    // A process inherits its working directory from its parent. This could be
    // on a mounted filesystem, which means that the running daemon would
    // prevent this filesystem from being unmounted. Changing to the root
    // directory avoids this problem.
    chdir("/");

    // The file mode creation mask is also inherited from the parent process.
    // We don't want to restrict the permissions on files created by the
    // daemon, so the mask is cleared.
    umask(0);

    // A second fork ensures the process cannot acquire a controlling terminal.
    //if (pid_t pid = fork())
    //{
    pid_t pid;
    if((pid = fork())){
		if (pid > 0)
		{
			exit(0);
		}
		else
		{
			syslog(LOG_ERR | LOG_USER, "Second fork failed: %m");
			exit(1);
		}
    }

    // Close the standard streams. This decouples the daemon from the terminal
    // that started it.
    close(0);
    close(1);
    close(2);

    // We don't want the daemon to have any standard input.
    if (open("/dev/null", O_RDONLY) < 0)
    {
		syslog(LOG_ERR | LOG_USER, "Unable to open /dev/null: %m");
		exit(1);
    }

    // Send standard output to a log file.
    //const char* output = "/var/log/flotilla.log";
    const int flags = O_WRONLY | O_CREAT | O_APPEND;
    const mode_t mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;
    if (open(log_file_path.c_str(), flags, mode) < 0)
    {
		syslog(LOG_ERR | LOG_USER, "Unable to open output file %s: %m", log_file_path.c_str());
		exit(1);
    }

    // Also send standard error to the same log file.
    if (dup(1) < 0)
    {
    	syslog(LOG_ERR | LOG_USER, "Unable to dup output descriptor: %m");
    	exit(1);
    }

}

/* Main */

int main(int argc, char *argv[])
{
	int i;
	running = 1;


	//safe_to_exit = 0;

	/*struct sigaction sigIntHandler;
	sigIntHandler.sa_handler = sigint_handler;
	sigemptyset(&sigIntHandler.sa_mask);
	sig_int_handler.sa_flags = 0;
	sigaction(SIGINT, &sigIntHandler, NULL);*/

	try {
		options_description desc("Flotilla Server\nAvailable options");

	    desc.add_options()
	        ("help,h", "Print this usage message")
	        ("port,p", value<int>(&flotilla_port), "Specify an alternate port")
#if defined(__linux__) || defined(__APPLE__)
	        ("pid-file", value<std::string>(&pid_file_path), "Specify an alternate pid file path")
	        ("log-file", value<std::string>(&log_file_path), "Specify an alternate log file path")
	    	("no-daemon,d", bool_switch(), "Prevent Flotilla from running as a daemon")
	    	("verbose,v", bool_switch(&be_verbose), "Start Flotilla verbosely")
	    ;
#endif

		variables_map vm;
	    store(parse_command_line(argc, argv, desc), vm);

	    if (vm.count("help")) {  
	        std::cout << desc << "\n";
	        return 0;
	    }

	    if (vm.count("pid-file")) {
	    	pid_file_path = vm["pid-file"].as<std::string>();
	    }

	    if (vm.count("log-file")) {
	    	log_file_path = vm["log-file"].as<std::string>();
	    }

	    if (vm.count("verbose")) {
	    	be_verbose = vm["verbose"].as<bool>();
	    }

#if defined(__linux__) || defined(__APPLE__)
		path bp_log_file(log_file_path);
		path bp_pid_file(pid_file_path);

		log_file_path = absolute(bp_log_file).string();
		pid_file_path = absolute(bp_pid_file).string();

		if(be_verbose){
			std::cout << "Using log file: " << log_file_path << std::endl;
			std::cout << "Using pid file: " << pid_file_path << std::endl;
		}
#endif

#if defined(__linux__) || defined(__APPLE__)
	    if (vm.count("no-daemon")) {
	    	should_daemonize = !vm["no-daemon"].as<bool>();
		}
#endif

	    if (vm.count("port")) { 
	    	flotilla_port = vm["port"].as<int>();
		}
	}
    catch(std::exception& e) {
        std::cerr << e.what() << std::endl;
        return 1;
    }

#if defined(__linux__) || defined(__APPLE__)
	if( should_daemonize ){

		std::ofstream pid_file;
		pid_file.open (pid_file_path, std::ofstream::out | std::ofstream::trunc);
		if(pid_file.is_open()){
			pid_file << ::getpid();

			if(pid_file.bad()){
				std::cerr << "Unable to write to pid file" << std::endl;
				return 1;
			}
			
			pid_file.close();
			pid_file.open (pid_file_path, std::ofstream::out | std::ofstream::trunc);
			daemonize();

			pid_file << ::getpid();

			pid_file.close();
		}
		else
		{
			std::cout << "Unable to open pid file" << std::endl;
			return 1;
		}
		std::cout << "Flotilla Started with PID: " << ::getpid() << std::endl;
	}
#endif

	signal(SIGINT, sigint_handler);
	signal(SIGTERM, sigint_handler);

	/*if (!SetConsoleCtrlHandler((PHANDLER_ROUTINE)CtrlHandler, TRUE)){
		std::cout << "Could not register Ctrl handler" << std::endl;
	}*/

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
	websocket_server.set_reuse_addr(FALSE);
	websocket_server.listen(boost::asio::ip::tcp::v4(), flotilla_port);

	std::cout << "Listening on port " << flotilla_port << std::endl;

	websocket_server.start_accept();
	websocket_server.run();

	std::cout << "Websocket Server Stopped, Cleaning Up..." << std::endl;

	cleanup();

	std::cout << "Bye bye!" << std::endl;

	//std::this_thread::sleep_for(std::chrono::microseconds(2000000));

	return 0;
}
