#pragma once
#include <iostream>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#if defined(_WIN32)
#include <windows.h>
#include <dbghelp.h>
#pragma comment(lib, "dbghelp.lib")
#else
#include <cxxabi.h>
#include <dlfcn.h>
#include <execinfo.h>
#include <link.h>
#endif

#ifndef OX_STACKTRACE_USE_OX_STRING
#include <string>
#define OX_INTERNAL_STACKTRACE_STRING std::string
#else
#error "ox.stacktrace: no support for ox.string"
#endif

#ifndef OX_STACKTRACE_USE_OX_VECTOR
#include <vector>
#define OX_INTERNAL_STACKTRACE_VECTOR std::vector
#else
#error "ox.stacktrace: no support for ox.vector"
#endif

namespace ox {
namespace details {
static OX_INTERNAL_STACKTRACE_STRING extract_filename(
    const OX_INTERNAL_STACKTRACE_STRING& path) {
    if (path.empty()) return "";

    size_t pos = path.find_last_of("/\\");
    if (pos == OX_INTERNAL_STACKTRACE_STRING::npos) return path;

    return path.substr(pos + 1);
}
}  // namespace details

struct stack_frame {
    void* address;
    OX_INTERNAL_STACKTRACE_STRING function_name;
    OX_INTERNAL_STACKTRACE_STRING raw_symbol;
    OX_INTERNAL_STACKTRACE_STRING file_name;
    unsigned long line;
    OX_INTERNAL_STACKTRACE_STRING module_name;
    OX_INTERNAL_STACKTRACE_STRING module_path;
    uintptr_t module_base;
    uintptr_t offset;
};

class stacktrace {
public:
    static OX_INTERNAL_STACKTRACE_VECTOR<stack_frame> capture(
        int max_frames = 62, int skip_frames = 1) {
#if defined(_WIN32)
        return capture_windows(max_frames, skip_frames);
#else
        return capture_unix(max_frames, skip_frames);
#endif
    }

    static void print(int skip_frames = 1) {
        const auto &frames = capture(62, skip_frames);
        std::cout << "stacktrace (most recent call first): " << std::endl;
        for (int i = 0; i < frames.size(); ++i) {
            const auto &frame = frames[i];
            std::cout << "#" << i << " " << std::hex << frame.address << " in ";
            print_function_name(frame);
            std::cout << " at " << (frame.file_name.empty() ? "<unknown>" : frame.file_name);
            if (frame.line != 0) {
                std::cout << ":" << frame.line;
            }
            std::cout << std::endl;
            std::cout.flush();
        }
    }

private:
    static void print_function_name(stack_frame const &frame) {
        if (frame.function_name.empty()) {
            if (frame.raw_symbol.empty()) {
                std::cout << "<unknown>";
            }
            std::cout << frame.raw_symbol;
        }
        std::cout << frame.function_name;
    }

private:
#if defined(_WIN32)
    static OX_INTERNAL_STACKTRACE_VECTOR<stack_frame> capture_windows(
        int max_frames, int skip_frames) {
        OX_INTERNAL_STACKTRACE_VECTOR<stack_frame> frames;

        HANDLE process = GetCurrentProcess();
        HANDLE thread = GetCurrentThread();

        SymInitialize(process, NULL, TRUE);
        SymSetOptions(SYMOPT_LOAD_LINES | SYMOPT_UNDNAME |
                      SYMOPT_DEFERRED_LOADS);

        CONTEXT context;
        RtlCaptureContext(&context);

        STACKFRAME64 stack_frame;
        memset(&stack_frame, 0, sizeof(STACKFRAME64));

#ifdef _M_IX86
        DWORD machine_type = IMAGE_FILE_MACHINE_I386;
        stack_frame.AddrPC.Offset = context.Eip;
        stack_frame.AddrPC.Mode = AddrModeFlat;
        stack_frame.AddrFrame.Offset = context.Ebp;
        stack_frame.AddrFrame.Mode = AddrModeFlat;
        stack_frame.AddrStack.Offset = context.Esp;
        stack_frame.AddrStack.Mode = AddrModeFlat;
#elif _M_X64
        DWORD machine_type = IMAGE_FILE_MACHINE_AMD64;
        stack_frame.AddrPC.Offset = context.Rip;
        stack_frame.AddrPC.Mode = AddrModeFlat;
        stack_frame.AddrFrame.Offset = context.Rbp;
        stack_frame.AddrFrame.Mode = AddrModeFlat;
        stack_frame.AddrStack.Offset = context.Rsp;
        stack_frame.AddrStack.Mode = AddrModeFlat;
#elif _M_ARM64
        DWORD machine_type = IMAGE_FILE_MACHINE_ARM64;
        stack_frame.AddrPC.Offset = context.Pc;
        stack_frame.AddrPC.Mode = AddrModeFlat;
        stack_frame.AddrFrame.Offset = context.Fp;
        stack_frame.AddrFrame.Mode = AddrModeFlat;
        stack_frame.AddrStack.Offset = context.Sp;
        stack_frame.AddrStack.Mode = AddrModeFlat;
#else
#error "Unsupported architecture"
#endif

        const size_t sym_info_size = sizeof(SYMBOL_INFO) + 256 * sizeof(char);
        SYMBOL_INFO* symbol = (SYMBOL_INFO*)malloc(sym_info_size);
        if (!symbol) {
            SymCleanup(process);
            return frames;
        }

        memset(symbol, 0, sym_info_size);
        symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
        symbol->MaxNameLen = 255;

        IMAGEHLP_LINE64 line_info;
        line_info.SizeOfStruct = sizeof(IMAGEHLP_LINE64);

        int frame_count = 0;
        while (frame_count < max_frames) {
            if (!StackWalk64(machine_type, process, thread, &stack_frame,
                             &context, NULL, SymFunctionTableAccess64,
                             SymGetModuleBase64, NULL)) {
                break;
            }

            if (frame_count++ < skip_frames) continue;

            if (stack_frame.AddrPC.Offset == 0) break;

            struct stack_frame frame;
            frame.address = reinterpret_cast<void*>(stack_frame.AddrPC.Offset);
            frame.function_name = "";
            frame.raw_symbol = "";
            frame.file_name = "";
            frame.line = 0;
            frame.module_name = "";
            frame.module_path = "";
            frame.module_base = 0;
            frame.offset = 0;

            DWORD64 module_base =
                SymGetModuleBase64(process, stack_frame.AddrPC.Offset);
            if (module_base) {
                frame.module_base = module_base;
                frame.offset = stack_frame.AddrPC.Offset - module_base;

                char module_path[MAX_PATH];
                if (GetModuleFileNameA((HMODULE)module_base, module_path,
                                       MAX_PATH)) {
                    frame.module_path = module_path;
                    frame.module_name = details::extract_filename(module_path);
                }
            }

            DWORD64 displacement = 0;
            if (SymFromAddr(process, stack_frame.AddrPC.Offset, &displacement,
                            symbol)) {
                frame.raw_symbol = symbol->Name;
                frame.function_name = symbol->Name;
            }

            DWORD line_displacement;
            if (SymGetLineFromAddr64(process, stack_frame.AddrPC.Offset,
                                     &line_displacement, &line_info)) {
                frame.file_name = line_info.FileName;
                frame.line = line_info.LineNumber;
            }

            frames.push_back(frame);
        }

        free(symbol);
        SymCleanup(process);
        return frames;
    }

#else
    struct module_info {
        uintptr_t base;
        uintptr_t size;
        OX_INTERNAL_STACKTRACE_STRING path;
    };

    static int module_collector(struct dl_phdr_info* info, size_t size,
                                void* data) {
        OX_INTERNAL_STACKTRACE_VECTOR<module_info>* modules =
            reinterpret_cast<OX_INTERNAL_STACKTRACE_VECTOR<module_info>*>(data);

        module_info mod;
        mod.base = info->dlpi_addr;
        mod.path = info->dlpi_name ? info->dlpi_name : "";

        mod.size = 0;
        for (int i = 0; i < info->dlpi_phnum; i++) {
            const ElfW(Phdr)& phdr = info->dlpi_phdr[i];
            if (phdr.p_type == PT_LOAD) {
                uintptr_t end = phdr.p_vaddr + phdr.p_memsz;
                if (end > mod.size) mod.size = end;
            }
        }

        modules->push_back(mod);
        return 0;
    }

    static OX_INTERNAL_STACKTRACE_VECTOR<stack_frame> capture_unix(
        int max_frames, int skip_frames) {
        OX_INTERNAL_STACKTRACE_VECTOR<stack_frame> frames;

        OX_INTERNAL_STACKTRACE_VECTOR<void*> stack(max_frames);
        int captured = backtrace(stack.data(), max_frames);

        OX_INTERNAL_STACKTRACE_VECTOR<module_info> modules;
        dl_iterate_phdr(module_collector, &modules);

        for (int i = skip_frames; i < captured; i++) {
            stack_frame frame;
            frame.address = stack[i];
            frame.function_name = "";
            frame.raw_symbol = "";
            frame.file_name = "";
            frame.line = 0;
            frame.module_name = "";
            frame.module_path = "";
            frame.module_base = 0;
            frame.offset = 0;

            uintptr_t addr = reinterpret_cast<uintptr_t>(stack[i]);

            Dl_info info;
            if (dladdr(stack[i], &info)) {
                if (info.dli_fname) {
                    frame.module_path = info.dli_fname;
                    frame.module_name =
                        details::extract_filename(info.dli_fname);
                }

                if (info.dli_fbase) {
                    frame.module_base =
                        reinterpret_cast<uintptr_t>(info.dli_fbase);
                    frame.offset = addr - frame.module_base;
                }

                if (info.dli_sname) {
                    frame.raw_symbol = info.dli_sname;

                    int status = 0;
                    char* demangled = abi::__cxa_demangle(
                        info.dli_sname, nullptr, nullptr, &status);

                    if (status == 0 && demangled) {
                        frame.function_name = demangled;
                        free(demangled);
                    } else {
                        frame.function_name = info.dli_sname;
                    }
                }
            }

            for (const auto& mod : modules) {
                if (addr >= mod.base && addr < (mod.base + mod.size)) {
                    if (!mod.path.empty()) {
                        frame.module_path = mod.path;
                        frame.module_name = details::extract_filename(mod.path);
                    }
                    frame.module_base = mod.base;
                    frame.offset = addr - mod.base;
                    break;
                }
            }

            frames.push_back(frame);
        }

        return frames;
    }
#endif
};

}  // namespace ox
