#include "session.hpp"

std::unique_ptr< session_manager > g_session_manager = std::make_unique<session_manager>();

std::shared_ptr< user_t > session_manager::create_session() {
	session_data.emplace_front(std::make_unique<user_t>());
	return session_data.front();
}

void session_manager::destroy_session(std::shared_ptr<user_t> a_session) {
	a_session->m_socket.disconnect();
}

std::shared_ptr<user_t> session_manager::get_front()
{
	return session_data.front();
}