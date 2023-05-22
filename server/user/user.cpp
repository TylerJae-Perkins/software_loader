#include "user.hpp"
#include "../session/session.hpp"
#include "../dependencies/json/json.hpp"

#include <logging.hpp>
#include <filesystem>
#include <Windows.h>
#include <fstream>
#include <SFML/Network.hpp>
#include <memory>
#include <thread>
#include <packet.h>
#include <network.h>

std::vector<char> cache_module(std::string_view a_module_name) {
	std::vector<char> m_data;
	std::fstream stream(a_module_name.data(), std::ios::in | std::ios::binary);
	if (!stream.is_open()) {
		LOG(std::format("failed to cache {}, file not found", a_module_name.data()), LOG_ERROR);
		return {};
	}

	stream.seekg(0, std::ios::end);
	m_data.reserve(static_cast<size_t>(stream.tellg()));
	stream.seekg(0, std::ios::beg);

	m_data.assign((std::istreambuf_iterator<char>(stream)),
		std::istreambuf_iterator<char>());
	stream.close();

	return m_data;
}

bool user_t::invite_user(std::string_view username, std::string_view password, std::string_view hwid) {
	LOG("New user being created!", LOG_WARNING);

	packet_t packet;
	sf::Socket::Status status = this->m_socket.receive(packet);

	if (status != sf::Socket::Done) {
		LOG("Failed to recieve invite code packet!", LOG_ERROR);
		return false;
	}

	std::string invite_code;
	packet >> invite_code;

	if (validate_invite(invite_code)) {
		create_user(username, password, hwid, invite_code);
	}

	return true;
}

bool user_t::create_client() {
	packet_t packet;

	sf::Socket::Status status = this->m_socket.receive(packet);

	if (status != sf::Socket::Done) {
		LOG("Failed to recieve login packet!", LOG_ERROR);
		return false;
	}

	std::string username, password, hwid;

	packet >> username;
	packet >> password;
	packet >> hwid;

	packet.clear();

	user_info_t user_info;

	std::pair<bool, std::string> has_user = get_user(username, password, hwid, user_info);
	if (!has_user.first) {
		packet << false;
		packet << "failed";
		packet << "failed";
		packet << has_user.second;
		status = this->m_socket.send(packet);
		if (status != sf::Socket::Done) {
			LOG("Failed to send info packet!", LOG_ERROR);
			return false;
		}

		packet.clear();
		status = this->m_socket.receive(packet);
		if (status != sf::Socket::Done) {
			LOG("Failed to recieve invite packet!", LOG_ERROR);
			return false;
		}

		bool should_invite = false;
		packet >> should_invite;

		if (should_invite)
			invite_user(username, password, hwid);

		packet.clear();
		return false;
	}

	packet << has_user.first;
	packet << user_info.hwid;
	packet << user_info.username;
	packet << has_user.second;

	status = this->m_socket.send(packet);

	if (status != sf::Socket::Done) {
		LOG("Failed to send info packet!", LOG_ERROR);
		return false;
	}
	packet.clear();

	// get utf-8 full path to config
	CHAR buffer[MAX_PATH] = { 0 };
	GetModuleFileNameA(NULL, buffer, MAX_PATH);
	std::string::size_type pos = std::string(buffer).find_last_of("\\/");
	std::string filepath = std::string(buffer).substr(0, pos);

	packet << cache_module(filepath + "\\cheat_dll.dll");
	status = this->m_socket.send(packet);

	if (status != sf::Socket::Done) {
		LOG("Failed to send cheat dll packet!", LOG_ERROR);
		return false;
	}
	packet.clear();


	return true;
}

void user_t::run_client() {
	m_thread = std::make_shared<sf::Thread>([&](std::shared_ptr<user_t> session) {
		packet_t packet;
		const sf::Socket::Status status = this->m_socket.receive(packet);

		if (status != sf::Socket::Done) {
			LOG("Failed to recieve loader version packet!", LOG_ERROR);
			g_session_manager->destroy_session(session);
			return;
		}

		if (!create_client())
			g_session_manager->destroy_session(session);

		g_session_manager->destroy_session(session);
		}, g_session_manager->get_front());

	m_thread->launch();
}

void user_t::create_invite(std::string_view invite_code) {
	std::filesystem::path file("invites.txt");

	// get utf-8 full path to config
	CHAR buffer[MAX_PATH] = { 0 };
	GetModuleFileNameA(NULL, buffer, MAX_PATH);
	std::string::size_type pos = std::string(buffer).find_last_of("\\/");
	std::string filepath = std::string(buffer).substr(0, pos);

	std::ifstream loaded_file;
	loaded_file.open(filepath / file, std::ifstream::binary);

	nlohmann::json json;
	if (loaded_file.is_open()) {
		loaded_file >> json;
	}

	if (json["Invite Codes"][invite_code.data()].is_null())
		json["Invite Codes"][invite_code.data()] = false;

	std::ofstream store_file;
	store_file.open(filepath / file, std::ios::out | std::ios::trunc);

	store_file << json.dump(4);
}

bool user_t::validate_invite(std::string_view invite_code) {
	std::filesystem::path file("invites.txt");

	// get utf-8 full path to config
	CHAR buffer[MAX_PATH] = { 0 };
	GetModuleFileNameA(NULL, buffer, MAX_PATH);
	std::string::size_type pos = std::string(buffer).find_last_of("\\/");
	std::string filepath = std::string(buffer).substr(0, pos);

	std::ifstream loaded_file;
	loaded_file.open(filepath / file, std::ifstream::binary);

	nlohmann::json json;
	if (loaded_file.is_open()) {
		loaded_file >> json;
		loaded_file.close();
	}
	else {
		LOG("invite file doesnt exist!", LOG_ERROR);
		return false;
	}

	if (json["Invite Codes"][invite_code.data()].is_null()) {
		LOG(std::format("Invite Code: {} Doesnt exist!", invite_code.data()), LOG_ERROR);
		return false;
	}

	if (json["Invite Codes"][invite_code.data()].get<bool>()) {
		LOG(std::format("Invite Code: {} Has been used!", invite_code.data()), LOG_WARNING);
		return false;
	}

	json["Invite Codes"][invite_code.data()] = true;

	std::ofstream offloaded_file;
	offloaded_file.open(filepath / file, std::ios::out | std::ios::trunc);

	offloaded_file << json.dump(4);

	return true;
}

void user_t::create_user(std::string_view username, std::string_view password, std::string_view hwid, std::string_view invite_code) {
	std::filesystem::path file("accounts.txt");

	// get utf-8 full path to config
	CHAR buffer[MAX_PATH] = { 0 };
	GetModuleFileNameA(NULL, buffer, MAX_PATH);
	std::string::size_type pos = std::string(buffer).find_last_of("\\/");
	std::string filepath = std::string(buffer).substr(0, pos);

	std::ifstream loaded_file;
	loaded_file.open(filepath / file, std::ifstream::binary);

	nlohmann::json json;
	if (loaded_file.is_open()) {
		loaded_file >> json;
	}

	json[username.data()]["Password"] = password.data();
	json[username.data()]["HWID"] = hwid.data();
	json[username.data()]["Invite Code"] = invite_code.data();

	std::string log_account;
	log_account += "create_user called on user: ";
	log_account += username.data();
	LOG(log_account, LOG_MESSAGE);

	std::ofstream store_file;
	store_file.open(filepath / file, std::ios::out | std::ios::trunc);

	store_file << json.dump(4);
}

std::pair<bool, std::string> user_t::get_user(std::string_view username, std::string_view password, std::string_view hwid, user_info_t& user_info) {
	std::filesystem::path file("accounts.txt");

	// get utf-8 full path to config
	CHAR buffer[MAX_PATH] = { 0 };
	GetModuleFileNameA(NULL, buffer, MAX_PATH);
	std::string::size_type pos = std::string(buffer).find_last_of("\\/");
	std::string filepath = std::string(buffer).substr(0, pos);

	std::ifstream loaded_file;
	loaded_file.open(filepath / file, std::ifstream::binary);

	nlohmann::json json;
	if (loaded_file.is_open()) {
		loaded_file >> json;
	}
	else {
		LOG("User file doesnt exist!", LOG_ERROR);
		return std::pair<bool, std::string>(false, "File Doesnt Exist");
	}

	if (json[username.data()].is_null()) {
		LOG(std::format("User: {} Doesnt exist!", username.data()), LOG_ERROR);
		return std::pair<bool, std::string>(false, "User Doesnt Exist");
	}

	if (strcmp(json[username.data()]["Password"].get<std::string>().data(), password.data()) == 0) {
		LOG(std::format("User: {} Password matches!", username.data()), LOG_MESSAGE);
	}
	else {
		LOG(std::format("User: {} Attempted to log in with invalid password!", username.data()), LOG_ERROR);
		return std::pair<bool, std::string>(false, "Invalid Password");
	}

	if (strcmp(json[username.data()]["HWID"].get<std::string>().data(), hwid.data()) == 0) {
		LOG(std::format("User: {} HWID Matches!", username.data()), LOG_MESSAGE);
	}
	else {
		LOG(std::format("User: {} Attempted to log in with an invalid hwid!", username.data()), LOG_ERROR);
		return std::pair<bool, std::string>(false, "Invalid HWID");
	}

	user_info = user_info_t(username.data(), hwid.data());

	return std::pair<bool, std::string>(true, "Success!");;
}