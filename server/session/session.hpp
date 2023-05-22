#pragma once

#include <memory>
#include <deque>
#include <thread>
#include <Windows.h>
#include <SFML/Network.hpp>
#include <sstream>
#include <iostream>

#include "../user/user.hpp"

class session_manager {
public:
	std::shared_ptr< user_t > create_session();
	void destroy_session(std::shared_ptr< user_t > session);
	std::shared_ptr<user_t> get_front();

	std::deque<std::shared_ptr<user_t>> session_data;
private:
};
extern std::unique_ptr< session_manager > g_session_manager;