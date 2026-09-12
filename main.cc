#include <stdio.h>
#include <stdlib.h>
#include <windows.h>

#include "diagnostics_client.h"

int wmain(int argc, wchar_t** argv) {
  if (argc != 2) {
    fwprintf(stderr, L"Usage: nomoreprofile.exe <process-id>\n");
    return 2;
  }

  wchar_t* end = nullptr;
  unsigned long process_id = wcstoul(argv[1], &end, 10);
  if (*argv[1] == L'\0' || *end != L'\0' || process_id > MAXDWORD) {
    fwprintf(stderr, L"[-] invalid process id\n");
    return 2;
  }

  wchar_t profiler_path[MAX_PATH];
  if (!GetModuleFileNameW(nullptr, profiler_path, ARRAYSIZE(profiler_path)))
    return 3;
  wchar_t* slash = wcsrchr(profiler_path, L'\\');
  if (slash == nullptr) return 3;
  wcscpy_s(slash + 1, ARRAYSIZE(profiler_path) - (slash - profiler_path + 1),
           L"nomoreprofile_profiler.dll");

  if (!nomoreprofile::AttachProfiler(static_cast<DWORD>(process_id),
                                     profiler_path, 15000)) {
    fwprintf(stderr, L"[-] profiler attach failed\n");
    return 1;
  }
  wprintf(L"[+] profiler attach request sent\n");
  return 0;
}
