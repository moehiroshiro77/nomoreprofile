#pragma once

#include <atomic>

#include "hook_profiler.h"

namespace nomoreprofile {

class ProfilerClassFactory final : public IClassFactory {
 public:
  ProfilerClassFactory();

  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** object) override;
  ULONG STDMETHODCALLTYPE AddRef() override;
  ULONG STDMETHODCALLTYPE Release() override;
  HRESULT STDMETHODCALLTYPE CreateInstance(IUnknown* outer, REFIID riid,
                                           void** object) override;
  HRESULT STDMETHODCALLTYPE LockServer(BOOL lock) override;

 private:
  std::atomic<ULONG> references_{1};
};

}  // namespace nomoreprofile
