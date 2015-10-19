#include "Flotilla.h"

FlotillaModule::FlotillaModule(){
	state = ModuleDisconnected;
}

/* A module is deemed volatile if we care about interim data.
For example, the Touch board is volatile since missing a data packet could miss a touch,
the dial isn't since its value is persistent at whatever point it's turned to */
bool FlotillaModule::is_volatile(void){
	if (name.compare("touch") == 0){
		return true;
	}
	return false;
}

void FlotillaModule::empty_queue(void){
	mutex.lock();
	if (command_queue.size() > 0){
		std::queue<std::string> empty;
		std::swap(command_queue, empty);
	}
	mutex.unlock();
}

void FlotillaModule::connect(std::string module_name){
	if (state == ModuleConnected) return;
	name = module_name;
	empty_queue();
	state = ModuleConnected;
}

void FlotillaModule::disconnect(void){
	state = ModuleDisconnected;
	empty_queue();
}

std::string FlotillaModule::get_next_command(void){
	std::string command = "";
	if (command_queue.size() > 0){
		mutex.lock();

		if (!is_volatile()){
			while (command_queue.size() > 1){ command_queue.pop(); }
		}

		command = command_queue.front();
		if (command_queue.size() > 1) {
			command_queue.pop();
		}
		mutex.unlock();
	}
	return command;
}

void FlotillaModule::queue_command(std::string cmd){
	if (cmd.empty()) return;
	mutex.lock();
	command_queue.push(cmd);
	mutex.unlock();
}

void FlotillaModule::queue_update(std::string cmd){
	if (cmd.empty()) return;
	mutex.lock();
	update_queue.push(cmd);
	mutex.unlock();
}

bool FlotillaModule::get_next_update(std::string &update){
	//std::string update = "";
	if (update_queue.size() > 0){
		mutex.lock();
		update = update_queue.front();
		if (update_queue.size() > 1) update_queue.pop();
		// Flush the outgoing buffer Client -> Dock for non volatile modules for which only the latest data is reelevant
		if (!is_volatile()){
			while (update_queue.size() > 1){
				update_queue.pop();
			}
		}
		mutex.unlock();
	}
	
	if (update.empty() || last_update.compare(update) == 0){
		//std::cout << "Update (" << update << ") same as last update (" << last_update << ")" << std::endl;
		return FALSE;
	}
	else
	{
		//std::cout << "Update (" << update << ") differs from last update (" << last_update << ")" << std::endl;
		last_update = update;
		return TRUE;
	}
}