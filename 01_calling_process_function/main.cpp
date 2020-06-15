#include <windows.h>

using PrintMessage_t = void ( * ) ( const char* );
using SumNumbers_t = int	( * )  ( int, int );

uintptr_t printMessage_offset   = 0x10E0;
uintptr_t sumNumbers_offset     = 0x1140;


void Start() {		

    uintptr_t module_base = reinterpret_cast<uintptr_t>( GetModuleHandle( nullptr ) );		

    PrintMessage_t printMessage = reinterpret_cast<PrintMessage_t>( module_base + printMessage_offset );
    SumNumbers_t   sumNumbers   = reinterpret_cast<SumNumbers_t>( module_base + sumNumbers_offset );

    printMessage( "Hello!!");
    sumNumbers( 7, 7 );
}


BOOL __stdcall DllMain( _In_ HINSTANCE hInstance, _In_ DWORD fdwReason, _In_ LPVOID lpvReserved ) {		//standard dll 

    switch( fdwReason ) {

    case DLL_PROCESS_ATTACH:
        HANDLE dummy_thread = CreateThread( nullptr, NULL, ( LPTHREAD_START_ROUTINE )Start, hInstance, NULL, nullptr );
        break;
    }
    return TRUE;
}