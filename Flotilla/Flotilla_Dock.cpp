#include <libserialport.h>
#include <sstream>
#include <iostream>

#include "Flotilla_Dock.h"
#include "Timestamp.h"

bool sp_wait_for(struct sp_port* port, std::string wait_for) {

	while (sp_output_waiting(port) > 0);

	auto start = std::chrono::high_resolution_clock::now();

	while (sp_readline(port).compare(wait_for) != 0) {
		if (std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now() - start).count() >= 100000) return false;
	};

	return true;

}

std::string sp_readline(struct sp_port* port) {

	std::string buffer = "";

	char c;

	/*
	Commands are sent from the host in hoofing great chunks one line
	at a time, so the chances of us hitting a timeout before getting our '\n'
	are slim.
	*/

	while (sp_blocking_read(port, &c, 1, 1000) > 0) {
		if (c == '\n') break;
		if (c != '\r') buffer += c;
	}

	return buffer;

}

FlotillaDock::FlotillaDock(void) {
	state = Disconnected;
	std::string name = "";
	std::string user = "";
	std::string serial = "";
	std::string version = "";
}

void FlotillaDock::queue_command(std::string command){
	mutex.lock();
	command_queue.push(command);
	mutex.unlock();
}

void FlotillaDock::tick(){
	if (state != Connected) return;
	int channel_index;

	mutex.lock();
	while (command_queue.size() > 0){
		sp_blocking_write(port, command_queue.front().c_str(), command_queue.front().length(), 0);
		command_queue.pop();
		std::this_thread::sleep_for(std::chrono::microseconds(100000));
	}
	mutex.unlock();

	for (channel_index = 0; channel_index < MAX_CHANNELS; channel_index++){
		if (module[channel_index].state != ModuleConnected) continue;

		std::string update;
		if (module[channel_index].get_next_update(update)){

			std::ostringstream stream;
			stream << "s " << (channel_index + 1) << " " << update;
			update = stream.str();

#ifdef DEBUG_TRANSPORT
			std::ostringstream msg;
			msg << GetTimestamp() << "Sending to dock: " << update << std::endl;
			std::cout << msg.str();
#endif

			sp_blocking_write(port, update.c_str(), update.length(), 0);
			sp_blocking_write(port, "\r", 1, 0);

			//std::this_thread::sleep_for(std::chrono::microseconds(100000));

		}
	}

	while (sp_input_waiting(port) > 0){
		process_command(sp_readline(port));
	}

}

void FlotillaDock::process_command(std::string command) {

	std::string::size_type size;

	switch (command.at(0)) {
	case '#': {

		if (command.find("User: ") != std::string::npos) {
			user = command.substr(8);

			std::ostringstream msg;
			msg << GetTimestamp() << "Dock: " << index << ", User Name: " << user << std::endl;
			std::cout << msg.str();

			return;
		}
		if (command.find("Dock: ") != std::string::npos) {
			name = command.substr(8);

			std::ostringstream msg;
			msg << GetTimestamp() << "Dock: " << index << ", Dock Name: " << name << std::endl;
			std::cout << msg.str();

			return;
		}

		std::ostringstream msg;
		msg << GetTimestamp() << "Dock: " << index << ", Debug: " << command.substr(2) << std::endl;
		std::cout << msg.str();
	} break;
	case 'u': {

		int channel = std::stoi(command.substr(2), &size) - 1;

		if (channel < MAX_CHANNELS) {

			std::string data = command.substr(size + 4 + module[channel].name.length());

			// Oh my! cout is messy and ugly and slow, there are good arguments for it, but I'm a big boy and can handle printf!
			//std::cout << GetTimestamp() << "Dock: " << index << ", Channel: " << channel << ", Name: " << module[channel].name << " Data: " << data << std::endl;
			//printf("Dock: %d, Channel: %d, Name: %s, Data: %s\n", index, channel, module[channel].name.c_str(), data.c_str());

			module[channel].queue_command(data);
		}

	} break;
	case 'd': { // Module Disconnect

		int channel = std::stoi(command.substr(2), &size) - 1;

		if (channel < MAX_CHANNELS) {

			std::string name = command.substr(size + 3);

			std::ostringstream msg;
			msg << GetTimestamp() << "Dock " << index << ", Ch " << channel << " Lost: " << name << std::endl;
			std::cout << msg.str();

			module[channel].disconnect();
			queue_module_event(channel);

		}

	} break;
	case 'c': { // Module Connect

		int channel = std::stoi(command.substr(2), &size) - 1;

		if (channel < MAX_CHANNELS) {

			std::string name = command.substr(size + 3);

			std::ostringstream msg;
			msg << GetTimestamp() << "Dock " << index << ", Ch " << channel << " Found: " << name << std::endl;
			std::cout << msg.str();

			module[channel].connect(name);
			queue_module_event(channel);

		}

	} break;
	}
}

void FlotillaDock::disconnect(void){
	if (state == Disconnected) return;

	state = Disconnected;

	int x;
	for (x = 0; x < MAX_CHANNELS; x++){
		if (module[x].state == ModuleConnected){
			module[x].disconnect();
			queue_module_event(x);
		}
	}

	sp_flush(port, SP_BUF_OUTPUT);
	sp_flush(port, SP_BUF_INPUT);

	sp_close(port);
	sp_free_port(port);

	std::ostringstream msg;
	msg << GetTimestamp() << "Dock Disconnected" << std::endl; // , serial " << serial << std::endl;
	std::cout << msg.str();
}

void FlotillaDock::cmd_enumerate(void){

	std::this_thread::sleep_for(std::chrono::microseconds(100000));
	sp_blocking_write(port, "e\r", 2, 0);

	std::ostringstream msg;
	msg << GetTimestamp() << "Enumerating Dock" << std::endl; // , serial " << serial << "..." << std::endl;
	std::cout << msg.str();
}

bool FlotillaDock::set_port(sp_port *new_port){
	if (state != Disconnected) return false;

	state = Connecting;
	if (sp_copy_port(new_port, &port) == SP_OK){

		const char* port_name = sp_get_port_name(port);

		sp_set_baudrate(port, BAUD_RATE);
		if (sp_open(port, SP_MODE_READ_WRITE) == SP_OK){
			if (get_version_info()){

				state = Connected;

				return true;
			}
			else
			{
				std::ostringstream msg;
				msg << GetTimestamp() << "Warning: Failed to get dock version information" << std::endl;
				std::cout << msg.str();
			}
		}
		else
		{
			std::ostringstream msg;
			msg << GetTimestamp() << "Warning: Failed to open port " << port_name << std::endl;
			std::cout << msg.str();
		}
	}
	else{
		std::ostringstream msg;
		msg << GetTimestamp() << "Warning: Failed to copy port!?" << std::endl;
		std::cout << msg.str();
	}

	state = Disconnected;
	return false;
}

std::vector<std::string> FlotillaDock::get_pending_commands(void){
	int channel_idx;
	std::vector<std::string> commands;

	for (channel_idx = 0; channel_idx < MAX_CHANNELS; channel_idx++){

		if (module[channel_idx].state != ModuleConnected) continue;

		std::string command = get_next_command(channel_idx);

		if (!command.empty()){
			commands.push_back(command);
		}
		
	}

	return commands;
}

std::string FlotillaDock::get_next_command(int channel){
	/*
	Retrieve the next command from the module on the specified channel
	and construct a valid client data packet including the host index
	*/
	std::ostringstream stream;
	std::string command = module[channel].get_next_command();

	if (!command.empty()){
		stream << "h:" << index << " d:u " << channel << "/" << module[channel].name << " " << command;
	}

	return stream.str();
}

bool FlotillaDock::has_pending_events(){
	return event_queue.size() > 0;
}


std::vector<std::string> FlotillaDock::get_pending_events(void){
	std::vector<std::string> events;
	mutex.lock();
	while (has_pending_events()){
		events.push_back(event_queue.front());
		event_queue.pop();
	}
	mutex.unlock();
	return events;
}

std::string FlotillaDock::get_next_event(){

	std::string event = "";
	mutex.lock();
	event = event_queue.front();
	event_queue.pop();
	mutex.unlock();

	return event;

}

std::string FlotillaDock::ident(){
	std::ostringstream stream;

	stream << "# Dock: ";
	stream << version << ",";
	stream << serial << ",";
	stream << user << ",";
	stream << name << ",";
	stream << index;

	return stream.str();
}

std::string FlotillaDock::module_event(int channel){
	std::ostringstream stream;

	stream << "h:" << index << "d:";

	if (module[channel].state == ModuleConnected){
		stream << "c";
	}
	else{
		stream << "d";
	}

	stream << " " << channel << "/" << module[channel].name;

	return stream.str();
}

void FlotillaDock::queue_module_event(int channel){
	mutex.lock();
	event_queue.push(module_event(channel));
	mutex.unlock();
}

bool FlotillaDock::get_version_info(){
	int x;

	version = "";
	//serial  = "";
	user = "";
	name = "";

	/*
	I've added a small delay here to counter a bug where the dock seems to hate receiving
	subsequent commands and doesn't respond to the second command at all.

	Can be tested simply by sending v\rv\r  in one transaction, this *should* return the version
	information twice, but the dock is dropping/ignoring the second request?

	Update: Fixed the dock in many ways to sideline module commands using an alternate parsing method
	however, rapid subsequent system commands like v ( version ), d ( debug ) will still make for
	an uphappy dock!
	*/

	while (sp_output_waiting(port) > 0);

	std::this_thread::sleep_for(std::chrono::microseconds(100000));

	sp_blocking_write(port, "v\r", 2, 0);

	if (!sp_wait_for(port, "# Flotilla ready to set sail..")) {
		return false;
	}

	std::ostringstream msg;
	msg << GetTimestamp() << "Dock Connected" << std::endl;
	std::cout << msg.str();
	msg.str(""); msg.clear();

	for (x = 0; x < 4; x++){
		std::string line = sp_readline(port);
		std::string::size_type position;

		if (line.length() > 0 && line.at(0) == '#'){

			if ((position = line.find("Version: ")) != std::string::npos){
				version = line.substr(position + 9);

				std::ostringstream report;
				report << GetTimestamp() << "Version: " << version << std::endl;
				std::cout << report.str();
			}

			if ((position = line.find("Serial: ")) != std::string::npos){
				serial = line.substr(position + 8);

				std::ostringstream report;
				report << GetTimestamp() << "Serial: " << serial << std::endl;
				std::cout << report.str();
			}

			if ((position = line.find("User: ")) != std::string::npos){
				user = line.substr(position + 6);

				std::ostringstream report;
				report << GetTimestamp() << "User: " << user << std::endl;
				std::cout << report.str();
			}

			if ((position = line.find("Dock: ")) != std::string::npos){
				name = line.substr(position + 6);

				std::ostringstream report;
				report << GetTimestamp() << "Name: " << name << std::endl;
				std::cout << report.str();
			}

		}

		line.clear();
	}

	/*
	The serial number is a hexadecimal representation of 11 bytes,
	so a valid serial string will *always* be 22 characters long.
	*/
	if (version.length() > 0 && serial.length() == 22){
		return true;
	}

	return false;
}
