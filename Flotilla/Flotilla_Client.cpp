#include "Flotilla.h"

FlotillaClient::FlotillaClient() {
	connected = false;
	ready = false;
}

void FlotillaClient::queue_command(std::string cmd) {
	if (cmd.empty()) return;
	mutex.lock();
	command_queue.push(cmd);
	mutex.unlock();
}

void FlotillaClient::empty_queue(void){
	mutex.lock();
	if (command_queue.size() > 0){
		std::queue<std::string> empty;
		std::swap(command_queue, empty);
	}
	mutex.unlock();
}