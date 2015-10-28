#include <thread>
#include <libserialport.h>
#include <iostream>
#include <map>
#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/server.hpp>

#include "Flotilla.h"

#ifndef _MAIN_H_
#define _MAIN_H_

#define PID 9220
#define VID 1003
#define FLOTILLA_PORT 9395

std::thread thread_dock_scan;
std::thread thread_update_clients;
std::thread thread_update_docks;

typedef std::map<websocketpp::connection_hdl, FlotillaClient, std::owner_less<websocketpp::connection_hdl>> t_client;

t_client clients;

FlotillaClient& get_data_from_hdl(websocketpp::connection_hdl hdl);

void worker_update_clients(void);
void worker_update_docks(void);

typedef websocketpp::server<websocketpp::config::asio> server;

server websocket_server;

void websocket_on_message(websocketpp::connection_hdl hdl, server::message_ptr msg);
void websocket_on_open(websocketpp::connection_hdl hdl);
void websocket_on_close(websocketpp::connection_hdl hdl);
void websocket_on_fail(websocketpp::connection_hdl hdl);

void cleanup(void);
void sigint_handler(int sig_num);

void scan_for_host(struct sp_port* port);

int main(int argc, char *argv[]);

bool running;
//bool safe_to_exit;

Flotilla flotilla;

#endif