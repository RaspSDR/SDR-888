#include <windows.h>
#include <dbghelp.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <numeric>
 

LONG WINAPI HandleCrash(EXCEPTION_POINTERS* exceptionPointers) {
    HANDLE process = GetCurrentProcess();
    HANDLE thread = GetCurrentThread();
    CONTEXT* context = exceptionPointers->ContextRecord;
 
    SymInitialize(process, nullptr, TRUE); // TRUE = load symbols from symbol path
 
    // Configure symbol options for better resolution
    SymSetOptions(SYMOPT_DEFERRED_LOADS | SYMOPT_LOAD_LINES | SYMOPT_UNDNAME);

    // Initialize stack frame for StackWalk64
    STACKFRAME64 stackFrame = {0};
    #ifdef _WIN64
        stackFrame.AddrPC.Offset = context->Rip; // 64-bit: RIP (instruction pointer)
        stackFrame.AddrPC.Mode = AddrModeFlat;
        stackFrame.AddrStack.Offset = context->Rsp; // 64-bit: RSP (stack pointer)
        stackFrame.AddrStack.Mode = AddrModeFlat;
        stackFrame.AddrFrame.Offset = context->Rbp; // 64-bit: RBP (base pointer)
        stackFrame.AddrFrame.Mode = AddrModeFlat;
    #else
        stackFrame.AddrPC.Offset = context->Eip; // 32-bit: EIP
        stackFrame.AddrPC.Mode = AddrModeFlat;
        stackFrame.AddrStack.Offset = context->Esp; // 32-bit: ESP
        stackFrame.AddrStack.Mode = AddrModeFlat;
        stackFrame.AddrFrame.Offset = context->Ebp; // 32-bit: EBP
        stackFrame.AddrFrame.Mode = AddrModeFlat;
    #endif
 
    // Buffer to store stack trace lines
    std::vector<std::string> stackTrace;
 
    // Walk the stack (max 100 frames to avoid infinite loops)
    for (int frameIndex = 0; frameIndex < 100; frameIndex++) {
        // Advance to the next stack frame
        BOOL success = StackWalk64(
            #ifdef _WIN64
                IMAGE_FILE_MACHINE_AMD64, // 64-bit architecture
            #else
                IMAGE_FILE_MACHINE_I386,  // 32-bit architecture
            #endif
            process,
            thread,
            &stackFrame,
            context,
            nullptr,
            SymFunctionTableAccess64,
            SymGetModuleBase64,
            nullptr
        );
 
        if (!success) break; // No more frames
 
        // Inside the StackWalk64 loop:
        DWORD64 frameAddress = stackFrame.AddrPC.Offset;
        
        // Buffer for symbol info (function name, etc.)
        char symbolBuffer[sizeof(SYMBOL_INFO) + 256] = {0};
        SYMBOL_INFO* symbolInfo = reinterpret_cast<SYMBOL_INFO*>(symbolBuffer);
        symbolInfo->SizeOfStruct = sizeof(SYMBOL_INFO);
        symbolInfo->MaxNameLen = 255;
        
        // Get function name and module base
        DWORD64 moduleBase = SymGetModuleBase64(process, frameAddress);
        BOOL symbolFound = SymFromAddr(process, frameAddress, nullptr, symbolInfo);
        
        // Get line number (if symbols are available)
        IMAGEHLP_LINE64 lineInfo = {0};
        lineInfo.SizeOfStruct = sizeof(IMAGEHLP_LINE64);
        DWORD displacement = 0;
        BOOL lineFound = SymGetLineFromAddr64(process, frameAddress, &displacement, &lineInfo);
 
        // Inside the StackWalk64 loop:
        std::stringstream frameLine;
        frameLine << "#" << frameIndex << ": ";
        
        // Get module name (e.g., MyApp.exe)
        char moduleName[MAX_PATH] = {0};
        if (moduleBase != 0) {
            GetModuleFileNameA(reinterpret_cast<HMODULE>(moduleBase), moduleName, MAX_PATH);
            frameLine << "[" << moduleName << "] ";
        }
        
        // Add function name (if symbol found)
        if (symbolFound) {
            frameLine << symbolInfo->Name << "() + 0x" << std::hex << displacement << std::dec;
        } else {
            frameLine << "0x" << std::hex << frameAddress << std::dec; // Fallback to address
        }
        
        // Add file and line number (if line info found)
        if (lineFound) {
            frameLine << " (" << lineInfo.FileName << ":" << lineInfo.LineNumber << ")";
        }
        
        stackTrace.push_back(frameLine.str());
    }
 
    ::MessageBoxA(nullptr, ("The application has crashed. Report bug with the data (Ctrl-C to Copy): \n" + std::accumulate(stackTrace.begin(), stackTrace.end(), std::string(), [](const std::string& a, const std::string& b) {
        return a + b + "\n";
    })).c_str(), "Application Crash", MB_OK | MB_ICONERROR);

    // Cleanup symbol handler
    SymCleanup(process);

    return EXCEPTION_EXECUTE_HANDLER; // Terminate the app after handling
}