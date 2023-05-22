#pragma once

#define SERVER_PORT 53000
#define SERVER_IP "107.181.136.216"
#define LDR_VERSION "53000" //use a string as it makes it literally impossible
							 //to know what the latest version is.

struct user_info_t {
	user_info_t() = default;
	user_info_t(std::string username, std::string hwid) :
		username(username),
		hwid(hwid)
	{}

	std::string username, hwid;
};