#pragma once
#ifndef _MAIN_H_
#define _MAIN_H_

#include <thread>
#include <libserialport.h>
#include <signal.h>
#if defined(__linux__) || defined(__APPLE__)
#include <syslog.h>
#endif
#include <iostream>
#include <fstream>
#include <cstdio>

#include <boost/program_options/value_semantic.hpp>
#include <boost/program_options.hpp>
#include <boost/filesystem.hpp>

#include "Flotilla.h"

std::thread thread_ip_notify;
std::thread thread_dock_scan;
std::thread thread_update_clients;
//std::thread thread_update_docks;

void worker_update_clients(void);
//void worker_update_docks(void);
void worker_dock_scan(void);
void worker_ip_notify(void);

void cleanup(void);
void sigint_handler(int sig_num);

void scan_for_host(struct sp_port* port);

int main(int argc, char *argv[]);

bool running;

#endif