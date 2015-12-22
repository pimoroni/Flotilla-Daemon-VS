#include "Timestamp.h"
#include <time.h>

std::string GetTimestamp(){
    std::time_t t = std::time(NULL);
    char mbstr[100];
    std::strftime(mbstr, sizeof(mbstr), "[%Y/%m/%d %H:%M:%S] ", std::localtime(&t));
    std::string s_time(mbstr);
    return s_time;
}
