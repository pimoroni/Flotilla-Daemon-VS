#include "Timestamp.h"
#include <time.h>

#if defined(__linux__) || defined(__APPLE__)
std::string GetTimestamp(){
    std::time_t t = std::time(NULL);
    char mbstr[100];
    std::strftime(mbstr, sizeof(mbstr), "[%Y/%m/%d %H:%M:%S] ", std::localtime(&t));
    std::string s_time(mbstr);
    return s_time;
}
#else
std::string GetTimestamp() {
	time_t t = time(NULL);
	char mbstr[100];
	struct tm timeinfo;
	localtime_s(&timeinfo, &t);
	strftime(mbstr, sizeof(mbstr), "[%Y/%m/%d %H:%M:%S] ", &timeinfo);
	std::string s_time(mbstr);
	return s_time;
}
#endif