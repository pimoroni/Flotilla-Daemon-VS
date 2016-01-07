#include <thread>
#include <libserialport.h>
#include <signal.h>
#if defined(__linux__) || defined(__APPLE__)
#include <syslog.h>
#endif
#include <iostream>
#include <fstream>
#include <cstdio>
#include <map>
#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/server.hpp>

#include <boost/program_options/value_semantic.hpp>
#include <boost/program_options.hpp>
#include <boost/filesystem.hpp>

#include "Flotilla.h"

#ifndef _MAIN_H_
#define _MAIN_H_

#define PID 0x08C3 //9220
#define VID 0x16D0 //1003
#define FLOTILLA_PORT 9395

using namespace boost::program_options;
using namespace boost::filesystem;

std::string pid_file_path = "/var/run/flotilla.pid";
std::string log_file_path = "/var/log/flotilla.log";
int flotilla_port = FLOTILLA_PORT;
bool should_daemonize = true;
bool be_verbose = false;

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