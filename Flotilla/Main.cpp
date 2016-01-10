#include "Main.h"
#include "Config.h"
#include "Timestamp.h"
#include "Discovery.h"

using namespace boost::program_options;
using namespace boost::filesystem;

std::string pid_file_path = PID_FILE_PATH;
std::string log_file_path = LOG_FILE_PATH;
int flotilla_port = FLOTILLA_PORT;
bool should_daemonize = true;
bool be_verbose = false;

void cleanup(void){
	int i;
	running = 0;

	thread_dock_scan.join();
	thread_update_clients.join();
	thread_update_docks.join();

	for (i = 0; i < MAX_DOCKS; i++){
		std::cout << GetTimestamp() << "Disconnecting Dock " << (i+1) << std::endl;
		flotilla.dock[i].disconnect();
	}

#if defined(__linux__) || defined(__APPLE__)
	if( should_daemonize ){
		std::remove(pid_file_path.c_str());
	}
#endif
}

void sigint_handler(int sig_num){
	flotilla.stop_server();
}

void update_connected_docks() {
	enum sp_return result;
	struct sp_port** ports = NULL;
	result = sp_list_ports(&ports);
	if (result != SP_OK) { return; };

	int x = 0;
	int y = 0;

	for (x = 0; x < MAX_DOCKS; x++) {

		if (flotilla.dock[x].port == NULL || flotilla.dock[x].state == Disconnected) {
			continue;
		};

		//std::cout << GetTimestamp() << "Checking for dock " << flotilla.dock[x].name << std::endl;

		const char* a = sp_get_port_name(flotilla.dock[x].port);
		bool found = false;

		while (ports[y] != NULL) {
			struct sp_port* port = ports[y];
			const char * b = sp_get_port_name(port);
			int usb_vid, usb_pid;

			sp_get_port_usb_vid_pid(port, &usb_vid, &usb_pid);

			//std::cout << GetTimestamp() << "Checking serial port " << b << std::endl;

			if (b != NULL && usb_vid == VID && usb_pid == PID && strcmp(a, b) == 0) {
				//std::cout << GetTimestamp() << "Found dock " << flotilla.dock[x].name << std::endl;
				found = true;
			}

			y++;
		}

		if (!found) {
			//std::cout << GetTimestamp() << "Dock " << (x+1) << " Disconnected" << std::endl;
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
	std::cout << GetTimestamp() << "Main.cpp: Checking port: " << port_name << " : " << port_desc << std::endl;
#endif

	if(sp_get_port_usb_vid_pid(port, &usb_vid, &usb_pid) != SP_OK){
#ifdef DEBUG_SCAN_FOR_HOST
        std::cout << GetTimestamp() << "Main.cpp: Could not get VID/PID" << std::endl;
#endif
        return;
    }
	
    if (usb_vid != VID || usb_pid != PID) {
		return;
	}

#ifdef DEBUG_SCAN_FOR_HOST
    std::cout << GetTimestamp() << "Main.cpp: Got details VID: " << usb_vid << " PID: " << usb_pid << std::endl;
#endif

	for (x = 0; x < MAX_DOCKS; x++) {
		if (flotilla.dock[x].state != Disconnected) {
			const char* existing_port_name = sp_get_port_name(flotilla.dock[x].port);
			if (strcmp(port_name, existing_port_name) == 0) {
#ifdef DEBUG_SCAN_FOR_HOST
				std::cout << GetTimestamp() << "Main.cpp: Found existing dock entry for " << port_name << std::endl;
#endif
				return;
			}
		}
	}

	//std::cout << GetTimestamp() << "Found port: " << port_desc << ", Name: " << port_name << std::endl;
	//std::cout << GetTimestamp() << "PID: " << usb_pid << " VID: " << usb_vid << std::endl;


	FlotillaDock temp;

	if (temp.set_port(port)){
		temp.disconnect();
#ifdef DEBUG_SCAN_FOR_HOST
		std::cout << GetTimestamp() << "Main.cpp: Successfully Identified Dock. Serial: " << temp.serial << std::endl;
#endif

		for (x = 0; x < MAX_DOCKS; x++){
			if (flotilla.dock[x].state == Disconnected){
#ifdef DEBUG_SCAN_FOR_HOST
				std::cout << GetTimestamp() << "Main.cpp: Using Dock slot at index " << x << std::endl;
#endif
				if (flotilla.dock[x].set_port(port)){
#ifdef DEBUG_SCAN_FOR_HOST
					std::cout << GetTimestamp() << "Main.cpp: Success! " << x << std::endl;
#endif
					//std::cout << GetTimestamp() << "Dock " << (x+1) << " Connected!" << std::endl;
					flotilla.dock[x].cmd_enumerate();
				};

				return;
			}
		}

	}

}

/* Update Threads */

void worker_update_clients(void){
	while (running){

		auto start = std::chrono::high_resolution_clock::now();

		int dock_idx;

		for (dock_idx = 0; dock_idx < MAX_DOCKS; dock_idx++){

			for (auto event : flotilla.dock[dock_idx].get_pending_events()){
				flotilla.send_to_clients(event);
			}

			if (flotilla.dock[dock_idx].state != Connected) continue;

			//std::cout << GetTimestamp() << "Dock Update: " << flotilla.dock[dock_idx].serial << std::endl;

			for (auto command : flotilla.dock[dock_idx].get_pending_commands()){
				flotilla.send_to_clients(command);
			}

		}

		auto elapsed = std::chrono::high_resolution_clock::now() - start;

		long long microseconds = std::chrono::duration_cast<std::chrono::microseconds>(elapsed).count();

		std::this_thread::sleep_for(std::chrono::microseconds(10000 - microseconds));
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


#if defined(__linux__) || defined(__APPLE__)
void daemonize(){
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
#endif

/* Main */

int main(int argc, char *argv[])
{
	int i;
	running = 1;

	discover_ipv4();

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
#endif
			;

		variables_map vm;
	    store(parse_command_line(argc, argv, desc), vm);

	    if (vm.count("help")) {  
	        std::cout << GetTimestamp() << desc << "\n";
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
			std::cout << GetTimestamp() << "Using log file: " << log_file_path << std::endl;
			std::cout << GetTimestamp() << "Using pid file: " << pid_file_path << std::endl;
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
			std::cout << GetTimestamp() << "Unable to open pid file" << std::endl;
			return 1;
		}
		std::cout << GetTimestamp() << "Flotilla Started with PID: " << ::getpid() << std::endl;
	}
#endif

	signal(SIGINT, sigint_handler);
	signal(SIGTERM, sigint_handler);

	thread_dock_scan = std::thread(worker_dock_scan);
	thread_update_clients = std::thread(worker_update_clients);
	thread_update_docks = std::thread(worker_update_docks);

	for (i = 0; i < MAX_DOCKS; i++){
		flotilla.dock[i].index = i;
	}

	std::cout << GetTimestamp() << "Flotilla Ready To Set Sail..." << std::endl;

	flotilla.setup_server(flotilla_port);

	std::cout << GetTimestamp() << "Baud rate " << BAUD_RATE << std::endl;

	std::cout << GetTimestamp() << "Listening on port " << flotilla_port << std::endl;

	flotilla.start_server();

	std::cout << GetTimestamp() << "Websocket Server Stopped, Cleaning Up..." << std::endl;

	cleanup();

	std::cout << GetTimestamp() << "Bye bye!" << std::endl;

	return 0;
}
