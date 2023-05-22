#pragma once

#include <string_view>
#include <SFML/Network.hpp>
#include <thread>
#include <network.h>
#include <utility>

struct user_t {
	std::shared_ptr<sf::Thread> m_thread;
	sf::TcpSocket m_socket{};

	bool invite_user(std::string_view username, std::string_view password, std::string_view hwid);

	bool create_client();
	void run_client();

	void create_invite(std::string_view invite_code);
	bool validate_invite(std::string_view invite_code);

	void create_user(std::string_view username, std::string_view password, std::string_view hwid, std::string_view invite_code);
	std::pair<bool, std::string> get_user(std::string_view username, std::string_view password, std::string_view hwid, user_info_t& user_info);
};