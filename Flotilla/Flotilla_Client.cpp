#include "Flotilla_Client.h"

FlotillaClient::FlotillaClient() {
	connected = false;
	ready = false;
}

bool FlotillaClient::subscribed_to(int dock_idx) {
	if (dock_subscription.find(dock_idx) != dock_subscription.end()) {
		return dock_subscription[dock_idx];
	}
	return false;
}

void FlotillaClient::subscribe(int dock_idx) {
	mutex.lock();
	dock_subscription[dock_idx] = true;
	mutex.unlock();
}

void FlotillaClient::unsubscribe(int dock_idx) {
	mutex.lock();
	dock_subscription[dock_idx] = false;
	mutex.unlock();
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