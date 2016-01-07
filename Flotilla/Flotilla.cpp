#include "Flotilla.h"

bool sp_wait_for(struct sp_port* port, std::string wait_for){


	//printf("\n\nWaiting for \"%s\"...\n", wait_for.c_str());

	while (sp_output_waiting(port) > 0);

	auto start = std::chrono::high_resolution_clock::now();

	while (sp_readline(port).compare(wait_for) != 0){
		if (std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now() - start).count() >= 10000) return false;
	};

	return true;

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

std::string sp_readline(struct sp_port* port){

	std::string buffer = "";

	char c;

	/*
	Commands are sent from the host in hoofing great chunks one line
	at a time, so the chances of us hitting a timeout before getting our '\n'
	are slim.
	*/

	while (sp_blocking_read(port, &c, 1, 1000) > 0){
		if (c == '\n') break;
		if (c != '\r') buffer += c;
	}

	return buffer;

}

Flotilla::Flotilla() {

}

