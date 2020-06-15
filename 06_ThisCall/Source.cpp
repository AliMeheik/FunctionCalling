#include <windows.h>
#include <TlHelp32.h>
#include <iostream>
#include <string>

constexpr uintptr_t functionAddress =0x00431040;
constexpr uintptr_t objAddress	= 0x00B7F7C3;



DWORD GetProcessID( const WCHAR *process_name ) {

	HANDLE processSnap_handle = CreateToolhelp32Snapshot( TH32CS_SNAPPROCESS, NULL );
	if( processSnap_handle != INVALID_HANDLE_VALUE ) {
		PROCESSENTRY32 process_buffer;
		process_buffer.dwSize = sizeof( PROCESSENTRY32 );

		if( Process32First( processSnap_handle, &process_buffer ) ) {
			do {
				if( !_wcsicmp( process_name, process_buffer.szExeFile ) ) {
					std::cout << "Process found" << std::endl;
					return process_buffer.th32ProcessID;
				}
			} while( Process32Next( processSnap_handle, &process_buffer ) );
			std::wcout << "Couldent find process: " << process_name << std::endl;
		}
	} else {
		std::cout << "Couldent get process snap handle" << std::endl;
		return -1;
	}

}

DWORD GetModuleBaseAddress( const WCHAR *module_name, INT *processID ) {
	HANDLE moduleSnap_handle = CreateToolhelp32Snapshot( TH32CS_SNAPMODULE, *processID );
	MODULEENTRY32 module_buffer;
	module_buffer.dwSize = sizeof( MODULEENTRY32 );
	if( Module32First( moduleSnap_handle, &module_buffer ) ) {
		do {
			if( !_wcsicmp( module_name, module_buffer.szModule ) ) {
				std::cout << "Module found returning base address! " << std::endl;
				return ( DWORD )module_buffer.modBaseAddr;
			}
		} while( Module32Next( moduleSnap_handle, &module_buffer ) );
		std::wcout << "Cant find module with module name: " << module_name << std::endl;
	}
	std::cout << "Error creating Module snap! " << std::endl;
	return -1;
}


struct _Args {
	DWORD function_address;
	DWORD object_address;
	int return_value;
};

struct DummyStruct {
	int x;
	int y;
	using memberFunction_pointer = int( __thiscall* ) ( void* obj,int a, int b);
	memberFunction_pointer functPointer = nullptr;
};



void MappedFunction( _Args *args ) {

	DummyStruct *ObjPointer = ( DummyStruct* )(args->object_address);		
	
	ObjPointer->x = 7;
	ObjPointer->y = 11;

	ObjPointer->functPointer = (DummyStruct::memberFunction_pointer)args->function_address;
	args->return_value = ObjPointer->functPointer( ObjPointer,20, 3);
}


void MarkerFunction2() {
	return;
}

int main() {
	_Args args{ 6,6 };

	std::wstring process_name;

	std::cout << "Enter process name: ";

	getline( std::wcin, process_name );

	INT process_id = GetProcessID( process_name.c_str() );

	HANDLE processHandle = OpenProcess( PROCESS_ALL_ACCESS, FALSE, process_id );
	if( processHandle == INVALID_HANDLE_VALUE ) {
		std::cout << "Error getting process handle" << std::endl;
		std::cin.get();
		return -1;
	}

	uintptr_t moduleBase_address = (uintptr_t)GetModuleBaseAddress( process_name.c_str(), &process_id );
	if( !moduleBase_address ) {
		std::cout << "Error getting module base address" << std::endl;
		std::cin.get();
		return -1;
	}


	args.function_address = functionAddress;
	args.object_address = objAddress;		//should be module + offset but i still dont know how to get the offset to the object address, so passing it as raw address



	long functSize = ( ( uintptr_t )MarkerFunction2 - ( uintptr_t )MappedFunction );

	functSize = abs( functSize );

	uintptr_t MaxSize = functSize + sizeof( _Args );


	uintptr_t allocatedMemoryAddr = ( uintptr_t )VirtualAllocEx( processHandle, nullptr, MaxSize, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE );
		

	if( !WriteProcessMemory( processHandle, ( LPVOID )allocatedMemoryAddr, ( LPCVOID )MappedFunction, functSize, nullptr ) ) {
		std::cout << "Error writing mapped function" << std::endl;
		std::cin.get();
		return -1;
	}
	
	if( !WriteProcessMemory( processHandle, ( LPVOID )(allocatedMemoryAddr+functSize), ( LPCVOID )&args, sizeof(_Args), nullptr ) ) {
		std::cout << "Error writing function arguments" << std::endl;
		std::cin.get();
		return -1;
	}

	uintptr_t argAddress = allocatedMemoryAddr + functSize;

	HANDLE threadHandle = CreateRemoteThread( processHandle, nullptr, NULL, ( LPTHREAD_START_ROUTINE )allocatedMemoryAddr, ( LPVOID )argAddress, NULL, NULL );

	if( !threadHandle ) {
		std::cout << "Error getting threadHandle or creating thread" << std::endl;
		std::cin.get();
		return -1;
	}

	DWORD returnedValue;

	WaitForSingleObject( threadHandle, INFINITE );
	GetExitCodeThread( threadHandle,&returnedValue );
	std::cout << "Thread exit code: " << returnedValue << std::endl;

	VirtualFreeEx( processHandle, (LPVOID)allocatedMemoryAddr, MaxSize, MEM_RELEASE );
	CloseHandle( threadHandle );
	CloseHandle( processHandle );

	std::cin.get();
	return 0;
}