#include <string>
#include <windows.h>
#include <TlHelp32.h>
#include <iostream>


uintptr_t functionOffset = 0x1140;
uintptr_t functionAddress;

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

uintptr_t GetModuleBaseAddress( const WCHAR* moduleName, int processID ) {
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

struct _Args {
    int a = 0;
    int b = 0;
    uintptr_t functionAddress;
    int return_value = 0;
};

void MappedFunction( _Args* pArgs ) {
    using SumNumber_t = int ( * )( int, int );
    SumNumber_t sumNumber = reinterpret_cast<SumNumber_t>( pArgs->functionAddress );	
    pArgs->return_value = sumNumber( pArgs->a, pArgs->b );
}


void MarkerFunction() {
    return;
}


int main() {
    HANDLE hProcess = nullptr;
    HANDLE hThread  = nullptr;
    uintptr_t funcMemAddress = 0;
    uintptr_t argMemAddress  = 0;
    uintptr_t totalSize      = 0;

    do {
        std::wstring processName;
        std::cout << "Enter process name: ";
        std::getline( std::wcin, processName );

        int process_id = GetProcessID( processName.c_str() );

        if( !process_id ) {
            std::cout << "Failed to find process id" << std::endl;
            break;
        }
        uintptr_t moduleBase = GetModuleBaseAddress( processName.c_str(), process_id );
        if( !moduleBase ) {
            std::cout << "Failed to get process base address" << std::endl;
            break;
        }

        hProcess = OpenProcess( PROCESS_ALL_ACCESS, FALSE, process_id );

        if( !hProcess ) {
            std::cout << "Failed to get process handle" << std::endl;
            break;
        }

        functionAddress = moduleBase + functionOffset;
        _Args args = { 3,3 };
        args.functionAddress = functionAddress;

        //size calc
        uintptr_t functSize = ( uintptr_t )MarkerFunction - ( uintptr_t )MappedFunction;
        totalSize = functSize + sizeof( _Args );
        std::cout << "Totalsize: " << totalSize << std::endl;

        funcMemAddress = ( uintptr_t )VirtualAllocEx( hProcess, 0, totalSize, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE );

        if( !funcMemAddress ) {
            std::cout << "Failed to allocate memory in target process" << std::endl;
            break;
        }

        if( !WriteProcessMemory( hProcess, ( LPVOID )funcMemAddress, ( LPCVOID )MappedFunction, functSize, nullptr ) ) {
            std::cout << "Failed to write mapped function to target process memory" << std::endl;
            break;
        }

        argMemAddress = funcMemAddress + functSize;

        if( !WriteProcessMemory( hProcess, ( LPVOID )argMemAddress, ( LPCVOID )&args, sizeof( args ), nullptr ) ) {
            std::cout << "Failed writing arguments to target process memory" << std::endl;
            break;
        }

        hThread = CreateRemoteThread( hProcess, nullptr, NULL, ( LPTHREAD_START_ROUTINE )funcMemAddress, ( LPVOID )argMemAddress, NULL, NULL );


        if( !hThread ) {
            std::cout << "Failed to create remote thread" << std::endl;
            break;
        }

        WaitForSingleObject( hThread, INFINITE );
        DWORD dwExit = 0;

        GetExitCodeThread( hThread, &dwExit );
        std::cout << "Thread exit code returned: " << dwExit << std::endl;

    } while( 0 );

    //cleanup
    if( funcMemAddress ) {
        VirtualFreeEx( hProcess, reinterpret_cast<LPVOID>( funcMemAddress ), totalSize, MEM_RELEASE );
    }

    if( hThread ) {
        CloseHandle( hThread );
    }

    if( hProcess ) {
        CloseHandle( hProcess );
    }

    std::cin.get();
    return true;
}
