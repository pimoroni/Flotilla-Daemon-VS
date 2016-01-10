#include <string>
#include <sstream>
#include <iostream>

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
	std::cout << "Discovered address: " << ipv4_addr << std::endl;
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

	return 0;
}
#endif