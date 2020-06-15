#include <windows.h>
#include <iostream>
#include <string>
#include <TlHelp32.h>

using std::cout;
using std::endl;

uintptr_t functionOffset = 0x10E0;

uintptr_t GetModuleBase( const WCHAR* moduleName, int processID ) {

    uintptr_t baseAddress = 0x0;
    MODULEENTRY32 mBuffer;
    mBuffer.dwSize = sizeof( MODULEENTRY32 );

    HANDLE hModuleSnap = CreateToolhelp32Snapshot( TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, processID );

    if( hModuleSnap != INVALID_HANDLE_VALUE ) {
        if( Module32First( hModuleSnap, &mBuffer ) ) {
            do {
                if( !_wcsicmp( moduleName, mBuffer.szModule ) ) {
                    baseAddress = reinterpret_cast< uintptr_t >( mBuffer.modBaseAddr );
                    break;
                }
            } while( Module32First( hModuleSnap, &mBuffer ) );
        }
        CloseHandle( hModuleSnap );
    }

    return baseAddress;
}

int GetProcessID( const WCHAR* processName ) {
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

int main() {

    HANDLE hThread = nullptr;
    HANDLE hProcess = nullptr;
    uintptr_t messageAddress = 0x0;


    do {

        std::wstring process_name;
        cout << "Enter process name: ";
        getline( std::wcin, process_name );

        int processID = GetProcessID( process_name.c_str() );

        if( !processID ) {
            cout << "Failed to find process id" << endl;
            break;
        }

        uintptr_t moduleBase = GetModuleBase( process_name.c_str(), processID );

        if( !moduleBase ) {
            cout << "Failed to find process base address" << endl;
            break;
        }

        uintptr_t functionAddress = moduleBase + functionOffset;

        hProcess = OpenProcess( PROCESS_ALL_ACCESS, FALSE, processID );

        if( !hProcess ) {
            cout << "Failed to get process handle" << endl;
            break;
        }

        const char* message = "Hi its me i was hiding!";
        int messageLength = strlen( message );

        messageAddress = ( uintptr_t )VirtualAllocEx( hProcess, nullptr, messageLength, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE );

        if( !messageAddress ) {
            cout << "Failed to allocate memory for our message" << endl;
            break;
        }

        if( !WriteProcessMemory( hProcess, ( LPVOID )messageAddress, ( LPCVOID )message, messageLength, nullptr ) ) {
            cout << "Failed to write" << endl;
            break;
        }

        hThread = CreateRemoteThread( hProcess, nullptr, NULL, ( LPTHREAD_START_ROUTINE )functionAddress, ( LPVOID )messageAddress, NULL, NULL );

        if( !hThread ) {
            cout << "Failed to create remote thread" << endl;
            break;
        }

        WaitForSingleObject( hThread, INFINITE );

    } while( 0 );


    if( messageAddress ) {
        VirtualFreeEx( hProcess, ( LPVOID )messageAddress, NULL, MEM_RELEASE );
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



