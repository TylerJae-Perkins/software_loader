#include <iostream>
#include <logging.hpp>
#include <Windows.h>
#include <network.h>
#include <SFML/Network.hpp>
#include <conio.h>
#include <d3d9.h>
#include <format>

#include "xor.h"
#include <packet.h>
#include <TlHelp32.h>
#include "../mapper/mapper.hpp"

#pragma comment (lib, "d3d9.lib")

namespace n_verification {
	std::string username;
	user_info_t user_info;
	sf::TcpSocket socket;

	std::string get_username() {
		std::string username;
		std::cout << xorstr("username: ");
		std::cin >> username;
		return username;
	}

	std::string hide_password() {
		std::string password;
		std::cout << xorstr("password: ");
		char c;
		while ((c = _getch()) != 13) {
			if (c == 8) {
				if (password.empty()) {
					continue;
				}

				password.pop_back();
				printf(xorstr("\b \b"));
				continue;
			}

			password += c;
			printf(xorstr("*"));
		}

		printf(xorstr("\n"));

		return password;
	}

	std::string get_hwid() {
		std::string hwid = "windows_hwid: ";

		HW_PROFILE_INFOA profile_info;
		if (GetCurrentHwProfileA(&profile_info) == 0) {
			return "";
		}

		hwid += profile_info.szHwProfileGuid;
		hwid += "; graphics_card: ";

		D3DADAPTER_IDENTIFIER9 adapter_identifier;
		Direct3DCreate9(D3D_SDK_VERSION)->GetAdapterIdentifier(D3DADAPTER_DEFAULT, 0, &adapter_identifier);
		hwid += adapter_identifier.Description;
		hwid += "; processor_cores: ";

		SYSTEM_INFO system_info;
		GetSystemInfo(&system_info);

		hwid += std::to_string(system_info.dwNumberOfProcessors);
		hwid += "; processor_type: ";
		hwid += std::to_string(system_info.dwProcessorType);
		hwid += "; processor_revision: ";
		hwid += std::to_string(system_info.wProcessorRevision);
		hwid += "; processor_architecture: ";
		hwid += std::to_string(system_info.wProcessorArchitecture);

		return hwid;
	}

	void use_invite() {
		packet_t packet;
		std::string invite_code;

		LOG_NO_BREAK("Invite Code: ", LOG_MESSAGE);
		std::cin >> invite_code;

		packet << invite_code;
		if (socket.send(packet) != sf::Socket::Done) {
			LOG("Failed to send invite code packet!", LOG_ERROR);
		}
		packet.clear();
	}

	void verification() {
		std::string ip = xorstr (SERVER_IP);
		sf::Socket::Status attempt_socket_connection = socket.connect(ip, SERVER_PORT, sf::seconds(1));
		if (attempt_socket_connection != sf::Socket::Done) {
			ip.clear();
			LOG(xorstr ("Unable to find server!"), LOG_ERROR);
			Sleep(1000);
			return;
		}
		ip.clear();

		packet_t packet;
		packet << LDR_VERSION;
		if (socket.send(packet) != sf::Socket::Done) {
			LOG(xorstr ("An error occured!"), LOG_ERROR);
			Sleep(1000);
			return;
		}
		packet.clear();

		//send username & password
		username = get_username();
		std::string password = hide_password();
		std::string hwid = get_hwid();

		packet << username << password << hwid;
		if (socket.send(packet) != sf::Socket::Done) {
			LOG(xorstr ("Unable to send username packet!"), LOG_ERROR);
			Sleep(1000);
			return;
		}
		packet.clear();

		sf::Socket::Status status = socket.receive(packet);
		if (status != sf::Socket::Done) {
			LOG(xorstr ("Unable to recieve verification packet!"), LOG_ERROR);
			Sleep(1000);
			return;
		}

		bool was_successful = false;
		std::string reason;

		packet >> was_successful;
		packet >> user_info.hwid;
		packet >> user_info.username;
		packet >> reason;
		packet.clear();

		if (!was_successful && (reason == xorstr("User Doesnt Exist") || reason == xorstr ("File Doesnt Exist"))) {
			LOG(xorstr ("This user doesnt exist!"), LOG_ERROR);
			LOG(xorstr ("Do you have a invite code to create an account? (y or n)"), LOG_MESSAGE);
			std::string y_or_n;
			std::cin >> y_or_n;
			bool should_create_invite = y_or_n == "y";

			packet << should_create_invite;
			if (socket.send(packet) != sf::Socket::Done) {
				LOG(xorstr ("Unable to send invite packet!"), LOG_ERROR);
				Sleep(1000);
				return;
			}
			packet.clear();

			if (should_create_invite)
				use_invite();

			Sleep(1000);
			return;
		}

		if (reason == xorstr ("Invalid Password")) {
			LOG(xorstr ("Password is invalid!"), LOG_ERROR);
			return;
		}

		if (reason == xorstr ("Invalid HWID")) {
			LOG(xorstr ("HWID is invalid! contact admin for reset!"), LOG_ERROR);
			return;
		}

		//disables console input so crackers cant freeze the thread by pressing console
		DWORD previous_mode;
		GetConsoleMode(GetStdHandle(STD_INPUT_HANDLE), &previous_mode);
		SetConsoleMode(GetStdHandle(STD_INPUT_HANDLE), ENABLE_EXTENDED_FLAGS | (previous_mode & ~ENABLE_QUICK_EDIT_MODE));

		LOG(std::format("User verified succesfully! Welcome {}!", user_info.username), LOG_MESSAGE);

		status = socket.receive(packet);
		if (status != sf::Socket::Done) {
			LOG(xorstr ("Unable to cheat packet!"), LOG_ERROR);
			Sleep(1000);
			return;
		}

		std::vector<char> module_byte;
		packet >> module_byte;

		packet.clear();

		if (!module_byte.empty()) {
			if (!FindWindowA(xorstr("Valve001"), NULL))
				system(xorstr ("start steam://rungameid/730"));

			while (!FindWindowA(xorstr ("Valve001"), NULL))
				std::this_thread::sleep_for(std::chrono::milliseconds(50));

			std::this_thread::sleep_for(std::chrono::milliseconds(2500));

			PROCESSENTRY32 process_entry{ 0 };
			process_entry.dwSize = sizeof(process_entry);

			HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
			if (snapshot == INVALID_HANDLE_VALUE)
				LOG(xorstr("unable to open csgo"), LOG_ERROR);

			DWORD pid = 0;
			BOOL to_return = Process32First(snapshot, &process_entry);
			while (to_return) {
				if (!strcmp(xorstr("csgo.exe"), process_entry.szExeFile)) {
					pid = process_entry.th32ProcessID;
					break;
				}

				to_return = Process32Next(snapshot, &process_entry);
			}

			HANDLE handle = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);

			if (g_module_mapper.map_module_to_process(module_byte, handle, 0)) {
				return;
			}
		}
	}
}

int main()
{
	n_verification::verification();
}