#include <windows.h>
#include <iostream>
#include <string>
#include <TlHelp32.h>

DWORD functionOffset = 0x10F0;

int GetProcessId( const  WCHAR* processName ) {
    int processId = 0;
    HANDLE hProcessSnap = CreateToolhelp32Snapshot( TH32CS_SNAPPROCESS, NULL );
    if( hProcessSnap != INVALID_HANDLE_VALUE ) {
        PROCESSENTRY32 p_Buffer;
        p_Buffer.dwSize = sizeof( PROCESSENTRY32 );
        if( Process32First( hProcessSnap, &p_Buffer ) ) {
            do {
                if( !_wcsicmp( processName, p_Buffer.szExeFile ) ) {
                    processId = p_Buffer.th32ProcessID;
                    break;
                }
            } while( Process32Next( hProcessSnap, &p_Buffer ) );
        }
        CloseHandle( hProcessSnap );
    }
    return processId;
}


DWORD GetModuleBase( const WCHAR* processName, DWORD processID ) {
    uintptr_t baseAddress = 0x0;
    MODULEENTRY32 mBuffer;
    mBuffer.dwSize = sizeof( MODULEENTRY32 );

    HANDLE hModuleSnap = CreateToolhelp32Snapshot( TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, processID );

    if( hModuleSnap != INVALID_HANDLE_VALUE ) {
        if( Module32First( hModuleSnap, &mBuffer ) ) {
            do {
                if( !_wcsicmp( processName, mBuffer.szModule ) ) {
                    baseAddress = reinterpret_cast< uintptr_t >( mBuffer.modBaseAddr );
                    break;
                }
            } while( Module32First( hModuleSnap, &mBuffer ) );
        }
        CloseHandle( hModuleSnap );
    }

    return baseAddress;
}


struct Args_t {
    int first = 0;
    int second = 0;
};


int main() {

    HANDLE	hProcess = nullptr;
    HANDLE  hThread = nullptr;
    int allocSize = 0;
    uintptr_t argsMemAddress = 0;

    do {

        Args_t arguments = { 10,10 };
        std::wstring processName;
        std::cout << "Enter process name: ";
        getline( std::wcin, processName );

        DWORD	processID = GetProcessId( processName.c_str() );
        if( !processID ) {
            std::cout << "Process not found" << std::endl;
            break;
        }
        DWORD	moduleBase = GetModuleBase( processName.c_str(), processID );

        if( !moduleBase ) {
            std::cout << "Invalid process base" << std::endl;
            break;
        }

        hProcess = OpenProcess( PROCESS_ALL_ACCESS, FALSE, processID );

        if( !hProcess ) {
            std::cout << "Failed opening process handle" << std::endl;
            break;
        }

        allocSize = 1000;				

        argsMemAddress = ( uintptr_t )VirtualAllocEx( hProcess, 0, allocSize, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE );

        if( !argsMemAddress ) {
            std::cout << "Failed to allocate memory in target process" << std::endl;
            break;
        }

        //32 bit 
        BYTE shellcode[] = {
            0xB8, 0x00, 0x00, 0x00,0x00,			// prepare placeholder for arguments address
            0xFF, 0x30,								// push [eax]				
            0xFF, 0x70, 0x04,						// push [eax+4]			
            0xB8, 0x00, 0x00, 0x00, 0x00,			// mov eax, func_address	
            0xFF, 0xD0,								// call eax				
            0x83, 0xC4, 0x08,						// add esp, 8		
            0xC3,									// return 
            0x90
        };

        *reinterpret_cast< uintptr_t* >( shellcode + 0x01 ) = argsMemAddress;
        *reinterpret_cast< uintptr_t* >( shellcode + 0x0B ) = ( functionOffset + moduleBase );


        if( !WriteProcessMemory( hProcess, ( LPVOID )argsMemAddress, ( LPCVOID )( &arguments ), sizeof( arguments ), nullptr ) ) {
            std::cout << "Failed to write arguments " << std::endl;
            break;
        }

        //write the function after the args
        uintptr_t func_address = ( uintptr_t )argsMemAddress + sizeof( arguments );

        if( !WriteProcessMemory( hProcess, ( LPVOID )func_address, ( LPCVOID )shellcode, sizeof( shellcode ), nullptr ) ) {
            std::cout << "Failed to write func call" << std::endl;
            break;
        }

        hThread = CreateRemoteThread( hProcess, 0, 0, ( LPTHREAD_START_ROUTINE )func_address, 0, 0, 0 );

        if( !hThread ) {
            std::cout << "Failed to create remote thread" << std::endl;
            break;
        }

        WaitForSingleObject( hThread, INFINITE );

    } while( 0 );

    if( argsMemAddress ) {
        VirtualFreeEx( hProcess, ( LPVOID )argsMemAddress, allocSize, MEM_RELEASE );
    }

    if( hThread ) {
        CloseHandle( hThread );
    }

    if( hProcess ) {
        CloseHandle( hProcess );
    }

    system( "pause" );
    return 0;
}

