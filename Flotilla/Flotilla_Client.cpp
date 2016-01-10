#include "Flotilla_Client.h"

FlotillaClient::FlotillaClient() {
	connected = false;
	ready = false;
}

FlotillaClient FlotillaClient::operator=(FlotillaClient src) {
	std::swap(connected, src.connected);
	std::swap(ready, src.ready);
	std::swap(command_queue, src.command_queue);
	return *this;
}

FlotillaClient::FlotillaClient(const FlotillaClient& src) :
	connected(src.connected),
	ready(src.ready),
	command_queue(src.command_queue),
mutex() {};

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