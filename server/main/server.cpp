#include <iostream>
#include <WinSock2.h>
#include <thread>
#include <SFML/Network.hpp>
	
#include "../dependencies/json/json.hpp"
#include "../user/user.hpp"
#include "../session/session.hpp"
#include <packet.h>

user_t user;

int main()
{
	DWORD previous_mode;
	GetConsoleMode(GetStdHandle(STD_INPUT_HANDLE), &previous_mode);
	SetConsoleMode(GetStdHandle(STD_INPUT_HANDLE), ENABLE_EXTENDED_FLAGS | (previous_mode & ~ENABLE_QUICK_EDIT_MODE));

	HANDLE std_output_handle = GetStdHandle(((DWORD)-11));
	SetConsoleTextAttribute(std_output_handle, 2);
	std::string_view text = "[MESSAGE] : Server is now active! \n";
	printf(text.data());

	sf::TcpListener listener;
	if (listener.listen(SERVER_PORT) != sf::Socket::Done) {
		
	}

	user.create_invite("8DHsWnRgDHmHYJU3");
	user.create_invite("qE5dQlxBa6vqzdsI");
	user.create_invite("okIqBxModDrCt8Ai");
	user.create_invite("0tYwrgEUirvWgsHp");
	user.create_invite("KBefjP5Kl8Ls3QLa");
	user.create_invite("KybvvccMwbb8CWT5");
	user.create_invite("h3zV4iEnjeXrrj6W");
	user.create_invite("23ZNDcMpzjRFkAW9");
	user.create_invite("9JPIljkDYFLcbvwq");
	user.create_invite("t9rSRSqoiCkmTNXb");
	user.create_invite("IUbed2x8d8RxnskT");

	while (true) {
		std::shared_ptr<user_t> cur_session = g_session_manager->create_session();

		if (listener.accept(cur_session->m_socket) != sf::Socket::Done) {
			printf("denied client! \n");
			continue;
		}
				
		cur_session->run_client();
	}
}