#include <string>
#include <sstream>
#include <iostream>
#include <boost/asio.hpp>
#include <iostream>
#include <sstream>

#include "Config.h"
#include "Discovery.h"
#include "Timestamp.h"

using boost::asio::ip::tcp;

#ifdef _WIN32

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0600
#endif
#include <stdio.h>
#include <winsock2.h>
#include <iphlpapi.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "iphlpapi.lib")

void discover_addr(std::string ipv4_addr) {
#ifdef NOTIFY_ENABLE
	http_notify_ipv4(ipv4_addr);
#endif
	std::cout << GetTimestamp() << "Discovered IPV4 address: " << ipv4_addr << std::endl;
}

bool win_enumerate_ipv4()
{
	DWORD rv, size;
	PIP_ADAPTER_ADDRESSES adapter_addresses, aa;
	PIP_ADAPTER_UNICAST_ADDRESS ua;

	rv = GetAdaptersAddresses(AF_UNSPEC, GAA_FLAG_INCLUDE_PREFIX, NULL, NULL, &size);
	if (rv != ERROR_BUFFER_OVERFLOW) {
		fprintf(stderr, "GetAdaptersAddresses() failed...");
		return false;
	}
	adapter_addresses = (PIP_ADAPTER_ADDRESSES)malloc(size);

	rv = GetAdaptersAddresses(AF_UNSPEC, GAA_FLAG_INCLUDE_PREFIX, NULL, adapter_addresses, &size);
	if (rv != ERROR_SUCCESS) {
		fprintf(stderr, "GetAdaptersAddresses() failed...");
		free(adapter_addresses);
		return false;
	}

	for (aa = adapter_addresses; aa != NULL; aa = aa->Next) {
		for (ua = aa->FirstUnicastAddress; ua != NULL; ua = ua->Next) {
			if (ua->Address.lpSockaddr->sa_family == AF_INET) {

				char buf[16]; // Getnameinfo requires "char" type

				getnameinfo(ua->Address.lpSockaddr, ua->Address.iSockaddrLength, buf, sizeof(buf), NULL, 0, NI_NUMERICHOST);

				/*
				We *really* don't care about addresses starting with 169 or 127
				so let's just skip them!
				*/
				if ((buf[0] == '1' && buf[1] == '6' && buf[2] == '9')
					|| (buf[0] == '1' && buf[1] == '2' && buf[2] == '7')) {
					continue;
				}

				std::ostringstream ipv4_addr;

				ipv4_addr << buf;

				discover_addr(ipv4_addr.str());
			}
		}
	}

	free(adapter_addresses);

	return true;
}
#endif

#if defined(__linux__) || defined(__APPLE__)
#include <sys/types.h>
#include <sys/socket.h>
#include <ifaddrs.h>
#include <netinet/in.h>
#include <net/if.h>

bool lin_enumerate_ipv4()
{
	ifaddrs* ifap = NULL;

	int result = getifaddrs(&ifap);
	int addr_count = 0;

	if(result != 0){
		// Non-zero result is an error
		return false;
	}

	ifaddrs* current = ifap;

	if(current == NULL){
		// No interface found
		return false;
	}

	while (current != NULL){

		if( current->ifa_addr->sa_family == AF_INET){
			const sockaddr_in* if_addr = reinterpret_cast<const sockaddr_in*>(current->ifa_addr);

			int addr = ntohl(if_addr->sin_addr.s_addr);

			if( ((addr >> 24) & 0xFF) != 169 && ((addr >> 24) & 0xFF) != 127 ){

				std::ostringstream stream;

				stream << ((addr >> 24) & 0xFF) << '.' << ((addr >> 16) & 0xFF) << '.' << ((addr >> 8) & 0xFF) << '.' << (addr & 0xFF);


				std::ostringstream comsg;
				comsg << GetTimestamp() << "DEBUG: Found IP address: " << stream.str() << std::endl;
				
				
				addr_count++;

			}

		}

		current = current->ifa_next;
	}

	return addr_count > 0;
}

#endif

int http_notify_ipv4(std::string ipv4) {


	try
	{
		boost::asio::io_service io_service;

		// Get a list of endpoints corresponding to the server name.
		tcp::resolver resolver(io_service);
		tcp::resolver::query query(NOTIFY_REQUEST_HOST, NOTIFY_REQUEST_PORT);
		tcp::resolver::iterator endpoint_iterator = resolver.resolve(query);

		// Try each endpoint until we successfully establish a connection.
		tcp::socket socket(io_service);
		boost::asio::connect(socket, endpoint_iterator);

		// Form the request. We specify the "Connection: close" header so that the
		// server will close the socket after transmitting the response. This will
		// allow us to treat all data up until the EOF as the content.
		boost::asio::streambuf request;
		std::ostream request_stream(&request);
		request_stream << "GET /add?ipv4=" << ipv4 << " HTTP/1.0\r\n";
		request_stream << "Host: " << NOTIFY_REQUEST_HOST << "\r\n";
		request_stream << "Accept: */*\r\n";
		request_stream << "Connection: close\r\n\r\n";

		// Send the request.
		boost::asio::write(socket, request);

		// Read the response status line. The response streambuf will automatically
		// grow to accommodate the entire line. The growth may be limited by passing
		// a maximum size to the streambuf constructor.
		boost::asio::streambuf response;
		boost::asio::read_until(socket, response, "\r\n");

		// Check that response is OK.
		std::istream response_stream(&response);
		std::string http_version;
		response_stream >> http_version;
		unsigned int status_code;
		response_stream >> status_code;
		std::string status_message;
		std::getline(response_stream, status_message);
		if (!response_stream || http_version.substr(0, 5) != "HTTP/")
		{
			std::cout << GetTimestamp() << "DEBUG: Invalid response" << std::endl;
			return 1;
		}
		if (status_code != 200)
		{
			std::cout << GetTimestamp() << "DEBUG: Response returned with status code " << status_code << std::endl;
			return 1;
		}

		// Read the response headers, which are terminated by a blank line.
		boost::asio::read_until(socket, response, "\r\n\r\n");
		// Process the response headers.
		std::string header;
		while (std::getline(response_stream, header) && header != "\r")
			std::cout << header << "\n";
		std::cout << "\n";

		// Write whatever content we already have to output.
		if (response.size() > 0)
			std::cout << &response;

		// Read until EOF, writing data to output as we go.
		boost::system::error_code error;
		while (boost::asio::read(socket, response,
			boost::asio::transfer_at_least(1), error))
			std::cout << &response;
		if (error != boost::asio::error::eof)
			throw boost::system::system_error(error);
	}
	catch (std::exception& e)
	{
		std::cout << GetTimestamp() << "DEBUG: Exception: " << e.what() << std::endl;
	}

	return true;

}

int discover_ipv4()
{
#ifdef _WIN32
	WSAData d;
	if (WSAStartup(MAKEWORD(2, 2), &d) != 0) {
		return -1;
	}
	win_enumerate_ipv4();
	WSACleanup();
#endif
#if defined(__linux__) || defined(__APPLE__)
	lin_enumerate_ipv4();
#endif

	return 0;
}