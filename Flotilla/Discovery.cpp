#include <string>
#include <sstream>
#include <iostream>
#include <sstream>
#include <cstdlib>
#include <vector>

#include <boost/date_time/gregorian/greg_month.hpp>
#include <boost/date_time/gregorian/gregorian.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/deadline_timer.hpp>
#include <boost/asio/io_service.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/read_until.hpp>
#include <boost/asio/streambuf.hpp>
#include <boost/system/system_error.hpp>
#include <boost/asio/write.hpp>
#include <boost/asio/read.hpp>
#include <boost/asio/error.hpp>
#include <boost/lambda/bind.hpp>
#include <boost/lambda/lambda.hpp>

#include "Config.h"
#include "Discovery.h"
#include "Timestamp.h"

using boost::asio::deadline_timer;
using boost::asio::ip::tcp;
using boost::lambda::bind;
using boost::lambda::var;
using boost::lambda::_1;

#ifdef _WIN32
#ifndef INET_ADDRSTRLEN
#define INET_ADDRSTRLEN 16
#endif
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0600
#endif
#include <stdio.h>
#include <winsock2.h>
#include <iphlpapi.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "iphlpapi.lib")

bool win_enumerate_ipv4(std::vector<std::string> &ipv4_addresses)
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

				char buf[INET_ADDRSTRLEN]; // Getnameinfo requires "char" type

				getnameinfo(ua->Address.lpSockaddr, ua->Address.iSockaddrLength, buf, sizeof(buf), NULL, 0, NI_NUMERICHOST);

				/*
				We *really* don't care about addresses starting with 169 or 127
				so let's just skip them!
				*/
				if ((buf[0] == '1' && buf[1] == '6' && buf[2] == '9')
					|| (buf[0] == '1' && buf[1] == '2' && buf[2] == '7')) {
					continue;
				}

				ipv4_addresses.push_back(std::string(buf));
			}
		}
	}

	free(adapter_addresses);

	return ipv4_addresses.size() > 0;
}
#endif

bool discover_addr(std::string ipv4_addr) {
	std::ostringstream msg;
	msg << GetTimestamp() << "Discovered IPV4 address: " << ipv4_addr << std::endl;
	std::cout << msg.str();
#ifdef NOTIFY_ENABLE
	return http_notify_ipv4(ipv4_addr);
#endif
	return false;
}

#if defined(__linux__) || defined(__APPLE__)
#include <sys/types.h>
#include <sys/socket.h>
#include <ifaddrs.h>
#include <netinet/in.h>
#include <net/if.h>

bool lin_enumerate_ipv4(std::vector<std::string> &ipv4_addresses)
{
	
	ifaddrs* ifap = NULL;

	int result = getifaddrs(&ifap);
	if(result != 0){
		// Non-zero result is an error
		return false;
	}

	ifaddrs* current;

	for (current = ifap; current != nullptr; current = current->ifa_next){
		if( current->ifa_addr == nullptr ){
			continue;
		}
		if( current->ifa_addr->sa_family == AF_INET){
			const sockaddr_in* if_addr = reinterpret_cast<const sockaddr_in*>(current->ifa_addr);
			char buf[INET_ADDRSTRLEN]; // 255.255.255.255\n
			inet_ntop(AF_INET, &(if_addr->sin_addr), buf, INET_ADDRSTRLEN);

			if ((buf[0] == '1' && buf[1] == '6' && buf[2] == '9')
			 || (buf[0] == '1' && buf[1] == '2' && buf[2] == '7')) {
				continue;
			}

			ipv4_addresses.push_back(std::string(buf));

		}

	}

	return ipv4_addresses.size() > 0;
}

#endif

class tcp_client
{
public:
	tcp_client()
		: socket_(io_service_),
		deadline_(io_service_)
	{
		// No deadline is required until the first socket operation is started. We
		// set the deadline to positive infinity so that the actor takes no action
		// until a specific deadline is set.
		deadline_.expires_at(boost::posix_time::pos_infin);

		// Start the persistent actor that checks for deadline expiry.
		check_deadline();
	}

	void connect(const std::string& host, const std::string& service,
		boost::posix_time::time_duration timeout)
	{
		// Resolve the host name and service to a list of endpoints.
		tcp::resolver::query query(host, service);
		tcp::resolver::iterator iter = tcp::resolver(io_service_).resolve(query);

		// Set a deadline for the asynchronous operation. As a host name may
		// resolve to multiple endpoints, this function uses the composed operation
		// async_connect. The deadline applies to the entire operation, rather than
		// individual connection attempts.
		deadline_.expires_from_now(timeout);

		// Set up the variable that receives the result of the asynchronous
		// operation. The error code is set to would_block to signal that the
		// operation is incomplete. Asio guarantees that its asynchronous
		// operations will never fail with would_block, so any other value in
		// ec indicates completion.
		boost::system::error_code ec = boost::asio::error::would_block;

		// Start the asynchronous operation itself. The boost::lambda function
		// object is used as a callback and will update the ec variable when the
		// operation completes. The blocking_udp_client.cpp example shows how you
		// can use boost::bind rather than boost::lambda.
		boost::asio::async_connect(socket_, iter, var(ec) = _1);

		// Block until the asynchronous operation has completed.
		do io_service_.run_one(); while (ec == boost::asio::error::would_block);

		// Determine whether a connection was successfully established. The
		// deadline actor may have had a chance to run and close our socket, even
		// though the connect operation notionally succeeded. Therefore we must
		// check whether the socket is still open before deciding if we succeeded
		// or failed.
		if (ec || !socket_.is_open())
			throw boost::system::system_error(
				ec ? ec : boost::asio::error::operation_aborted);
	}

	std::string read_line(boost::posix_time::time_duration timeout)
	{
		// Set a deadline for the asynchronous operation. Since this function uses
		// a composed operation (async_read_until), the deadline applies to the
		// entire operation, rather than individual reads from the socket.
		deadline_.expires_from_now(timeout);

		// Set up the variable that receives the result of the asynchronous
		// operation. The error code is set to would_block to signal that the
		// operation is incomplete. Asio guarantees that its asynchronous
		// operations will never fail with would_block, so any other value in
		// ec indicates completion.
		boost::system::error_code ec = boost::asio::error::would_block;

		// Start the asynchronous operation itself. The boost::lambda function
		// object is used as a callback and will update the ec variable when the
		// operation completes. The blocking_udp_client.cpp example shows how you
		// can use boost::bind rather than boost::lambda.
		boost::asio::async_read_until(socket_, input_buffer_, '\n', var(ec) = _1);

		// Block until the asynchronous operation has completed.
		do io_service_.run_one(); while (ec == boost::asio::error::would_block);

		if (ec)
			throw boost::system::system_error(ec);

		std::string line;
		std::istream is(&input_buffer_);
		std::getline(is, line);
		return line;
	}


	void write_line(const std::string& line,
		boost::posix_time::time_duration timeout)
	{
#ifdef _DISCOVERY_DEBUG
		std::ostringstream msg;
		msg << "DEBUG: Request: " << line << std::endl;
		std::cout << msg.str();
#endif

		std::string data = line + "\n";

		// Set a deadline for the asynchronous operation. Since this function uses
		// a composed operation (async_write), the deadline applies to the entire
		// operation, rather than individual writes to the socket.
		deadline_.expires_from_now(timeout);

		// Set up the variable that receives the result of the asynchronous
		// operation. The error code is set to would_block to signal that the
		// operation is incomplete. Asio guarantees that its asynchronous
		// operations will never fail with would_block, so any other value in
		// ec indicates completion.
		boost::system::error_code ec = boost::asio::error::would_block;

		// Start the asynchronous operation itself. The boost::lambda function
		// object is used as a callback and will update the ec variable when the
		// operation completes. The blocking_udp_client.cpp example shows how you
		// can use boost::bind rather than boost::lambda.
		boost::asio::async_write(socket_, boost::asio::buffer(data), var(ec) = _1);

		// Block until the asynchronous operation has completed.
		do io_service_.run_one(); while (ec == boost::asio::error::would_block);

		if (ec)
			throw boost::system::system_error(ec);
	}

private:
	void check_deadline()
	{
		// Check whether the deadline has passed. We compare the deadline against
		// the current time since a new asynchronous operation may have moved the
		// deadline before this actor had a chance to run.
		if (deadline_.expires_at() <= deadline_timer::traits_type::now())
		{
			// The deadline has passed. The socket is closed so that any outstanding
			// asynchronous operations are cancelled. This allows the blocked
			// connect(), read_line() or write_line() functions to return.
			boost::system::error_code ignored_ec;
			socket_.close(ignored_ec);

			// There is no longer an active deadline. The expiry is set to positive
			// infinity so that the actor takes no action until a new deadline is set.
			deadline_.expires_at(boost::posix_time::pos_infin);
		}

		// Put the actor back to sleep.
		deadline_.async_wait(bind(&tcp_client::check_deadline, this));
	}

	boost::asio::io_service io_service_;
	tcp::socket socket_;
	deadline_timer deadline_;
	boost::asio::streambuf input_buffer_;
};


bool http_notify_ipv4(std::string ipv4) {

	try
	{
		tcp_client ip_notify_client;
		ip_notify_client.connect(NOTIFY_REQUEST_HOST, NOTIFY_REQUEST_PORT, boost::posix_time::seconds(10));

		std::ostringstream http_get_request;

		http_get_request << "GET /add?ipv4=" << ipv4 << " HTTP/1.1\r";
		ip_notify_client.write_line(http_get_request.str(), boost::posix_time::seconds(10));
		http_get_request.clear();
		http_get_request.str("");

		http_get_request << "Host: " << NOTIFY_REQUEST_HOST << "\r";
		ip_notify_client.write_line(http_get_request.str(), boost::posix_time::seconds(10));
		http_get_request.clear();
		http_get_request.str("");

		ip_notify_client.write_line("Accept: */*\r", boost::posix_time::seconds(10));
		ip_notify_client.write_line("Connection: close\r", boost::posix_time::seconds(10));
		ip_notify_client.write_line("\r", boost::posix_time::seconds(10));

		std::string response = ip_notify_client.read_line(boost::posix_time::seconds(10));
#ifdef _DISCOVERY_DEBUG
		std::cout << "DEBUG: Response: " << response << std::endl;
#endif

		if (response.substr(0,5) != "HTTP/") {
			return false;
		}

		for (;;)
		{
			response = ip_notify_client.read_line(boost::posix_time::seconds(10));
#ifdef _DISCOVERY_DEBUG
			std::cout << "DEBUG: Response: " << response << std::endl;
#endif
			if (response.substr(0, 2) == "ok") {
				std::ostringstream msg;
				msg << GetTimestamp() << "IPV4 Address " << ipv4 << " registered successfully" << std::endl;
				std::cout << msg.str();
				return true;
			}
		}
	}
	catch (std::exception& e)
	{
		std::ostringstream msg;
		msg << GetTimestamp() << "Discovery Exception: " << e.what() << std::endl;
		std::cout << msg.str();
	}

	return false;

}

int discover_ipv4(std::string &canonical_address)
{
	std::vector<std::string> ipv4_addresses;
	int result;

#ifdef _WIN32
	WSAData d;
	if (WSAStartup(MAKEWORD(2, 2), &d) != 0) {
		return 0;
	}
	result = win_enumerate_ipv4(ipv4_addresses);
	WSACleanup();
#endif
#if defined(__linux__) || defined(__APPLE__)
	result = lin_enumerate_ipv4(ipv4_addresses);
#endif

	int notify_count = 0;
	if (result > 0) {
		canonical_address = ipv4_addresses.front();
		for (auto ipv4_address : ipv4_addresses) {
			if (discover_addr(ipv4_address)) {
				notify_count++;
			}
		}
	}

	return ipv4_addresses.size();
}
