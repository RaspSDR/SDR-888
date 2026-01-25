#include <core.h>

#ifdef _WIN32
#include <windows.h>
LONG WINAPI HandleCrash(EXCEPTION_POINTERS* exceptionPointers);
#endif

int main(int argc, char* argv[]) {

#ifdef _WIN32
    SetUnhandledExceptionFilter(HandleCrash);
#endif

    return sdrpp_main(argc, argv);
}