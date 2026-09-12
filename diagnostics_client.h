#pragma once

#include <windows.h>

namespace nomoreprofile {

bool AttachProfiler(DWORD process_id, const wchar_t* profiler_path,
                    DWORD timeout_ms);

}  // namespace nomoreprofile
