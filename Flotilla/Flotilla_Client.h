#pragma once
#ifndef _FLOTILLA_CLIENT_H
#define _FLOTILLA_CLIENT_H

#include "Config.h"
#include <mutex>
#include <string>
#include <queue>
#include <map>

class FlotillaClient {
public:
	bool connected;	// Flag to indicate client has connected, will usually be true except in odd circumstances, possibly unecessary
	bool ready = false;     // Flag to indicate client ready status, module updates should not be sent unless client is ready
	std::map <int, bool> dock_subscription;
	void queue_command(std::string cmd);
	FlotillaClient();
	FlotillaClient(const FlotillaClient& src) :
		connected(src.connected),
		ready(src.ready),
		command_queue(src.command_queue),
		dock_subscription(src.dock_subscription),
		mutex() {};
	FlotillaClient& operator=(FlotillaClient src) {
		std::swap(connected, src.connected);
		std::swap(ready, src.ready);
		std::swap(command_queue, src.command_queue);
		return *this;
	};
	bool FlotillaClient::subscribed_to(int dock_idx);
	void FlotillaClient::subscribe(int dock_idx);
	void FlotillaClient::unsubscribe(int dock_idx);
private:
	void empty_queue(void);
	std::mutex mutex;
	std::queue <std::string> command_queue;
};

#endif