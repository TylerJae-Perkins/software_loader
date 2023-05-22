#pragma once
#include <Windows.h>
#include <string>
#include <vector>
#include <ctime>
#include <thread>
#include <fstream>
#include <TlHelp32.h>
#include <filesystem>

using load_library_a_fn = HINSTANCE(WINAPI*)(const char* module_file_name);
using get_process_address_fn = UINT_PTR(WINAPI*)(HINSTANCE module, const char* proc_name);
using dll_entry_point_fn = BOOL(WINAPI*)(void* module_handle, DWORD reason, void* reserved);

struct module_mapper_data {
	load_library_a_fn load_library_a;
	get_process_address_fn get_process_address;
	HINSTANCE module;
	int key;
};

class module_mapper {
public:
	bool map_module_to_process(std::vector<char> module_bytes, HANDLE process_handle, int key);
};
extern module_mapper g_module_mapper;