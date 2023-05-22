#ifndef LOGGING_HPP
#define LOGGING_HPP

#include <string_view>
#include <Windows.h>

enum e_log_type {
	LOG_MESSAGE = 0,
	LOG_WARNING,
	LOG_ERROR
};

namespace n_logger {
	void log_to_console(std::string_view string, e_log_type log, bool create_new_line = true) {
		HANDLE std_output_handle = GetStdHandle(((DWORD)-11));

		std::string text = "";
		switch (log) {
		case LOG_MESSAGE:
			SetConsoleTextAttribute(std_output_handle, 2);
			text = "[MESSAGE] : ";
			break;
		case LOG_WARNING:
			SetConsoleTextAttribute(std_output_handle, 14);
			text = "[WARNING] : ";
			break;
		case LOG_ERROR:
			SetConsoleTextAttribute(std_output_handle, 12);
			text = "[ERROR]   : ";
			break;
		}

		text += string.data();

		if (create_new_line)
			text += "\n";

		printf(text.data());
	}
}

//log_type = int | 0 = Message, 1 = Warning, 2 = Error
#define LOG(str, log_type) n_logger::log_to_console(str, log_type);
#define LOG_NO_BREAK(str, log_type) n_logger::log_to_console(str, log_type, false);

#endif