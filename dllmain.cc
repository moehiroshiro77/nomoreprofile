#define _OLE32_
#include <cstdio>
#include <cstring>

#include "class_factory.h"

namespace {
void OpenProfilerConsole() {
  if (!AllocConsole()) return;
  FILE* stream = nullptr;
  freopen_s(&stream, "CONOUT$", "w", stdout);
  freopen_s(&stream, "CONOUT$", "w", stderr);
}
}  // namespace

using nomoreprofile::OfflineProfileUnlockProfiler;
using nomoreprofile::ProfilerClassFactory;

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID reserved) {
  (void)reserved;
  if (reason == DLL_PROCESS_ATTACH) {
    DisableThreadLibraryCalls(instance);
    OpenProfilerConsole();
  }
  return TRUE;
}

#if defined(_M_IX86)
#pragma comment(linker, "/export:DllGetClassObject=_DllGetClassObject@12")
#pragma comment(linker, "/export:DllCanUnloadNow=_DllCanUnloadNow@0")
#else
#pragma comment(linker, "/export:DllGetClassObject")
#pragma comment(linker, "/export:DllCanUnloadNow")
#endif

extern "C" HRESULT STDAPICALLTYPE DllGetClassObject(REFCLSID clsid, REFIID riid,
                                                    LPVOID* object) {
  if (object == nullptr) return E_POINTER;
  *object = nullptr;

  const GUID targetClsid = OfflineProfileUnlockProfiler::GetClsid();
  if (std::memcmp(&clsid, &targetClsid, sizeof(GUID)) != 0)
    return CLASS_E_CLASSNOTAVAILABLE;

  auto* factory = new ProfilerClassFactory();
  if (factory == nullptr) return E_OUTOFMEMORY;

  const HRESULT hr = factory->QueryInterface(riid, object);
  factory->Release();
  return hr;
}

extern "C" HRESULT STDAPICALLTYPE DllCanUnloadNow() { return S_FALSE; }
