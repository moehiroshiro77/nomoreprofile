#pragma once

#include "profiler.h"

namespace nomoreprofile {

class OfflineProfileUnlockProfiler final : public Profiler {
 public:
  static GUID GetClsid();

  HRESULT STDMETHODCALLTYPE Initialize(IUnknown* infoUnknown) override;
  HRESULT STDMETHODCALLTYPE InitializeForAttach(IUnknown* infoUnknown,
                                                void* clientData,
                                                UINT clientDataSize) override;
  HRESULT STDMETHODCALLTYPE ProfilerAttachComplete() override;
  HRESULT STDMETHODCALLTYPE ModuleLoadFinished(ModuleID moduleId,
                                               HRESULT hrStatus) override;
  HRESULT STDMETHODCALLTYPE
  GetReJITParameters(ModuleID moduleId, mdMethodDef methodId,
                     ICorProfilerFunctionControl* functionControl) override;
  HRESULT STDMETHODCALLTYPE Shutdown() override;
  HRESULT STDMETHODCALLTYPE JITCompilationStarted(FunctionID functionId,
                                                  BOOL safeToBlock) override;

  // Used by FunctionIDMapper2 to decide which methods receive enter/leave
  // callbacks.
  bool IsTarget(FunctionID functionId, ModuleID* moduleId,
                mdMethodDef* methodId);

 private:
  enum class TargetKind { None, Current, Legacy };

  ModuleID target_module_id_ = 0;
  mdMethodDef target_method_id_ = mdTokenNil;
  TargetKind target_kind_ = TargetKind::None;
  HRESULT ReplaceWithTrue(ModuleID moduleId, mdMethodDef methodId);
  HRESULT ApplyTargetRejit();
  HRESULT TryFindTarget(ModuleID moduleId);
  HRESULT ApplyLegacyRejit(ModuleID moduleId, mdMethodDef methodId,
                           ICorProfilerFunctionControl* functionControl);
};

}  // namespace nomoreprofile
