#pragma once
#ifndef _FLOTILLA_CLIENT_H
#define _FLOTILLA_CLIENT_H

#include <mutex>
#include <string>
#include <queue>

class FlotillaClient {
public:
	bool connected;	// Flag to indicate client has connected, will usually be true except in odd circumstances, possibly unecessary
	bool ready;     // Flag to indicate client ready status, module updates should not be sent unless client is ready
	void queue_command(std::string cmd);
	FlotillaClient();
	FlotillaClient(const FlotillaClient& src) :
		connected(src.connected),
		ready(src.ready),
		command_queue(src.command_queue),
		mutex() {};
	FlotillaClient& operator=(FlotillaClient src) {
		std::swap(connected, src.connected);
		std::swap(ready, src.ready);
		std::swap(command_queue, src.command_queue);
		return *this;
	}
private:
	void empty_queue(void);
	std::mutex mutex;
	std::queue <std::string> command_queue;
};

#endif