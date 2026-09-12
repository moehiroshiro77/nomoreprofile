#include "class_factory.h"

namespace nomoreprofile {

ProfilerClassFactory::ProfilerClassFactory() = default;

HRESULT STDMETHODCALLTYPE ProfilerClassFactory::QueryInterface(REFIID riid,
                                                               void** object) {
  if (object == nullptr) return E_POINTER;
  *object = nullptr;

  if (riid == IID_IUnknown || riid == IID_IClassFactory) {
    *object = static_cast<IClassFactory*>(this);
    AddRef();
    return S_OK;
  }
  return E_NOINTERFACE;
}

ULONG STDMETHODCALLTYPE ProfilerClassFactory::AddRef() {
  return references_.fetch_add(1, std::memory_order_relaxed) + 1;
}

ULONG STDMETHODCALLTYPE ProfilerClassFactory::Release() {
  const ULONG count = references_.fetch_sub(1, std::memory_order_acq_rel) - 1;
  if (count == 0) delete this;
  return count;
}

HRESULT STDMETHODCALLTYPE ProfilerClassFactory::CreateInstance(IUnknown* outer,
                                                               REFIID riid,
                                                               void** object) {
  if (object == nullptr) return E_POINTER;
  *object = nullptr;
  if (outer != nullptr) return CLASS_E_NOAGGREGATION;

  auto* profiler = new OfflineProfileUnlockProfiler();
  if (profiler == nullptr) return E_OUTOFMEMORY;

  // QueryInterface transfers the initial reference to the CLR.  Do not
  // release it here: the object starts with a zero reference count.
  return profiler->QueryInterface(riid, object);
}

HRESULT STDMETHODCALLTYPE ProfilerClassFactory::LockServer(BOOL lock) {
  return S_OK;
}

}  // namespace nomoreprofile
