//caling function with inline asm, dll injection
#include <windows.h>

uintptr_t functionAddress = 0x10F0;

DWORD __stdcall Start( LPVOID param ) {

    uintptr_t moduleBaseAddr = reinterpret_cast<uintptr_t>( GetModuleHandle( NULL ) );
    functionAddress += moduleBaseAddr;

    __asm {
        push 5  
        push 5	
        mov eax, functionAddress	
        call eax	//call eax -> our function
        add esp, 8	//clean 
    }

    return 0;
}



BOOL __stdcall DllMain( _In_ void* _DllHandle, _In_ unsigned long _Reason, _In_opt_ void* _Reserved ) {

    switch( _Reason ) {
    case DLL_PROCESS_ATTACH:
        HANDLE hThread = CreateThread( 0, 0, Start, _DllHandle, 0, 0 );
        break;
    }

    return TRUE;
}



