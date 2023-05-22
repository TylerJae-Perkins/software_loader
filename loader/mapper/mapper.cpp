#include "mapper.hpp"
#include <iostream>
#include "../main/lazy_importer.h"

void __stdcall shellcode(module_mapper_data* data);

bool module_mapper::map_module_to_process(std::vector<char> module_bytes, HANDLE process_handle, int key) {
	SetFocus(FindWindowA("Valve001", NULL));

	IMAGE_NT_HEADERS* old_nt_header = nullptr;
	IMAGE_OPTIONAL_HEADER* old_optional_nt_header = nullptr;
	IMAGE_FILE_HEADER* old_file_header = nullptr;
	BYTE* target_base = nullptr;

	old_nt_header = reinterpret_cast<IMAGE_NT_HEADERS*>(module_bytes.data() + reinterpret_cast<IMAGE_DOS_HEADER*>(module_bytes.data())->e_lfanew);
	old_optional_nt_header = &old_nt_header->OptionalHeader;
	old_file_header = &old_nt_header->FileHeader;

	if (old_file_header->Machine != IMAGE_FILE_MACHINE_I386) {
		module_bytes.clear();
		return false;
	}

	target_base = reinterpret_cast<BYTE*>(api(VirtualAllocEx)(process_handle, reinterpret_cast<void*>(old_optional_nt_header->ImageBase), old_optional_nt_header->SizeOfImage, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE));
	if (!target_base) {
		target_base = reinterpret_cast<BYTE*>(api(VirtualAllocEx)(process_handle, nullptr, old_optional_nt_header->SizeOfImage, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE));
		if (!target_base) {
			module_bytes.clear();
			return false;
		}
	}

	SYSTEMTIME time;
	api(GetSystemTime)(&time);

	module_mapper_data data{ 0 };
	data.load_library_a = LoadLibraryA;
	data.get_process_address = reinterpret_cast<get_process_address_fn>(GetProcAddress);
	data.key = key;

	auto* section_header = IMAGE_FIRST_SECTION(old_nt_header);
	for (UINT i = 0; i != old_file_header->NumberOfSections; ++i, ++section_header) {
		if (section_header->SizeOfRawData) {
			if (!api(WriteProcessMemory)(process_handle, target_base + section_header->VirtualAddress, module_bytes.data() + section_header->PointerToRawData, section_header->SizeOfRawData, nullptr)) {
				module_bytes.clear();
				api(VirtualFreeEx)(process_handle, target_base, 0, MEM_RELEASE);
				return false;
			}
		}
	}

	api(memcpy)(module_bytes.data(), &data, sizeof(data));
	api(WriteProcessMemory)(process_handle, target_base, module_bytes.data(), 0x1000, nullptr);

	module_bytes.clear();

	void* _shellcode = api(VirtualAllocEx)(process_handle, nullptr, 0x1000, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
	if (!_shellcode) {
		api(VirtualFreeEx)(process_handle, target_base, 0, MEM_RELEASE);
		return false;
	}

	api(WriteProcessMemory)(process_handle, _shellcode, shellcode, 0x1000, nullptr);

	HANDLE thread_handle = api(CreateRemoteThread)(process_handle, nullptr, 0, reinterpret_cast<LPTHREAD_START_ROUTINE>(_shellcode), target_base, 0, nullptr);
	if (!thread_handle) {
		api(VirtualFreeEx)(process_handle, target_base, 0, MEM_RELEASE);
		api(VirtualFreeEx)(process_handle, _shellcode, 0, MEM_RELEASE);
		return false;
	}

	api(CloseHandle)(thread_handle);

	HINSTANCE module_check = NULL;
	while (!module_check) {
		module_mapper_data data_checked{ 0 };
		api(ReadProcessMemory)(process_handle, target_base, &data_checked, sizeof(data_checked), nullptr);

		module_check = data_checked.module;
	}

	api(VirtualFreeEx)(process_handle, _shellcode, 0, MEM_RELEASE);
	return true;
}

#define reloc_flag32(rel_info) ((rel_info >> 0x0C) == IMAGE_REL_BASED_HIGHLOW)
#define reloc_flag64(rel_info) ((rel_info >> 0x0C) == IMAGE_REL_BASED_DIR64)

#ifdef _WIN64
#define reloc_flag reloc_flag64
#else
#define reloc_flag reloc_flag32
#endif

void __stdcall shellcode(module_mapper_data* data) {
	if (!data)
		return;

	BYTE* base = reinterpret_cast<BYTE*>(data);
	auto* optional = &reinterpret_cast<IMAGE_NT_HEADERS*>(base + reinterpret_cast<IMAGE_DOS_HEADER*>(data)->e_lfanew)->OptionalHeader;

	auto _load_library_a = data->load_library_a;
	auto _get_proc_address = data->get_process_address;
	auto _dll_main = reinterpret_cast<dll_entry_point_fn>(base + optional->AddressOfEntryPoint);
	auto key = data->key;

	BYTE* location_delta = base - optional->ImageBase;
	if (location_delta) {
		if (!optional->DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC].Size)
			return;

		auto* reloc_data = reinterpret_cast<IMAGE_BASE_RELOCATION*>(base + optional->DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC].VirtualAddress);
		while (reloc_data->VirtualAddress) {
			UINT amount_of_entries = (reloc_data->SizeOfBlock - sizeof(IMAGE_BASE_RELOCATION)) / sizeof(WORD);
			WORD* relative_info = reinterpret_cast<WORD*>(reloc_data + 1);

			for (UINT i = 0; i != amount_of_entries; ++i, ++relative_info) {
				if (reloc_flag(*relative_info)) {
					UINT_PTR* patch = reinterpret_cast<UINT_PTR*>(base + reloc_data->VirtualAddress + ((*relative_info) & 0xFFF));
					*patch += reinterpret_cast<UINT_PTR>(location_delta);
				}
			}

			reloc_data = reinterpret_cast<IMAGE_BASE_RELOCATION*>(reinterpret_cast<BYTE*>(reloc_data) + reloc_data->SizeOfBlock);
		}
	}

	if (optional->DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].Size) {
		auto* import_description = reinterpret_cast<IMAGE_IMPORT_DESCRIPTOR*>(base + optional->DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress);
		while (import_description->Name) {
			char* module_str = reinterpret_cast<char*>(base + import_description->Name);
			HINSTANCE module = _load_library_a(module_str);

			ULONG_PTR* thunk_ref = reinterpret_cast<ULONG_PTR*>(base + import_description->OriginalFirstThunk);
			ULONG_PTR* func_ref = reinterpret_cast<ULONG_PTR*>(base + import_description->FirstThunk);

			if (!thunk_ref)
				thunk_ref = func_ref;

			for (; *thunk_ref; ++thunk_ref, ++func_ref) {
				if (IMAGE_SNAP_BY_ORDINAL(*thunk_ref))
					*func_ref = _get_proc_address(module, reinterpret_cast<char*>(*thunk_ref & 0xFFFF));
				else {
					auto* p_import = reinterpret_cast<IMAGE_IMPORT_BY_NAME*>(base + (*thunk_ref));
					*func_ref = _get_proc_address(module, p_import->Name);
				}
			}

			++import_description;
		}
	}

	if (optional->DataDirectory[IMAGE_DIRECTORY_ENTRY_TLS].Size) {
		auto* tls = reinterpret_cast<IMAGE_TLS_DIRECTORY*>(base + optional->DataDirectory[IMAGE_DIRECTORY_ENTRY_TLS].VirtualAddress);
		auto* callback = reinterpret_cast<PIMAGE_TLS_CALLBACK*>(tls->AddressOfCallBacks);
		for (; callback && *callback; ++callback)
			(*callback)(base, DLL_PROCESS_ATTACH, nullptr);
	}

	_dll_main(base, DLL_PROCESS_ATTACH, reinterpret_cast<void*>(key));

	data->module = reinterpret_cast<HINSTANCE>(base);
}

module_mapper g_module_mapper;