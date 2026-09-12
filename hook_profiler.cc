#include "hook_profiler.h"

#include <atomic>
#include <cstdio>

#include "ilrewriter.h"

namespace nomoreprofile {
namespace {
constexpr UINT_PTR targetCallbackId =
    static_cast<UINT_PTR>(~static_cast<UINT_PTR>(0));
std::atomic<OfflineProfileUnlockProfiler*> activeProfiler{nullptr};
std::atomic<bool> profilerActive{false};

void Log(const char* message) {
  std::printf("%s\n", message);
  std::fflush(stdout);
}

void __cdecl LogTargetEnter() { Log("[*] CanCreateOtherProfile() invoked"); }

void __cdecl LogTargetLeave() {
  Log("[+] CanCreateOtherProfile() returned true");
}

UINT_PTR __stdcall FunctionIdMapper(FunctionID functionId, void* clientData,
                                    BOOL* hookFunction) {
  (void)clientData;
  if (hookFunction == nullptr) return 0;

  *hookFunction = FALSE;
  auto* profiler = activeProfiler.load(std::memory_order_acquire);
  if (profiler == nullptr || !profilerActive.load(std::memory_order_acquire))
    return 0;

  ModuleID moduleId = 0;
  mdMethodDef methodId = mdTokenNil;
  if (!profiler->IsTarget(functionId, &moduleId, &methodId)) return 0;

  *hookFunction = TRUE;
  return targetCallbackId;
}

void __cdecl EnterCallback(FunctionIDOrClientID functionId) {
  SHUTDOWNGUARD_RETVOID();
  if (!profilerActive.load(std::memory_order_acquire) ||
      functionId.clientID != targetCallbackId)
    return;

  Log("[*] CanCreateOtherProfile() invoked");
}

void __cdecl LeaveCallback(FunctionIDOrClientID functionId) {
  SHUTDOWNGUARD_RETVOID();
  if (!profilerActive.load(std::memory_order_acquire) ||
      functionId.clientID != targetCallbackId)
    return;

  Log("[+] CanCreateOtherProfile() returned true");
}
}  // namespace

GUID OfflineProfileUnlockProfiler::GetClsid() {
  // {F62D2CE1-6B64-4C2F-9C2C-9D693BCE8A31}
  return {0xF62D2CE1,
          0x6B64,
          0x4C2F,
          {0x9C, 0x2C, 0x9D, 0x69, 0x3B, 0xCE, 0x8A, 0x31}};
}

HRESULT STDMETHODCALLTYPE
OfflineProfileUnlockProfiler::Initialize(IUnknown* infoUnknown) {
  HRESULT hr = Profiler::Initialize(infoUnknown);
  if (FAILED(hr) || pCorProfilerInfo == nullptr)
    return FAILED(hr) ? hr : E_FAIL;

  const DWORD eventMask =
      COR_PRF_MONITOR_JIT_COMPILATION | COR_PRF_MONITOR_ENTERLEAVE |
      COR_PRF_DISABLE_INLINING | COR_PRF_DISABLE_ALL_NGEN_IMAGES;

  hr = pCorProfilerInfo->SetEventMask2(eventMask, 0);
  if (FAILED(hr)) {
    std::printf("nomoreprofile: SetEventMask2 failed: 0x%08lx\n",
                static_cast<unsigned long>(hr));
    return hr;
  }

  activeProfiler.store(this, std::memory_order_release);
  hr = pCorProfilerInfo->SetFunctionIDMapper2(FunctionIdMapper, nullptr);
  if (FAILED(hr)) {
    activeProfiler.store(nullptr, std::memory_order_release);
    std::printf("nomoreprofile: SetFunctionIDMapper2 failed: 0x%08lx\n",
                static_cast<unsigned long>(hr));
    return hr;
  }

  hr = pCorProfilerInfo->SetEnterLeaveFunctionHooks3(EnterCallback,
                                                     LeaveCallback, nullptr);
  if (FAILED(hr)) {
    activeProfiler.store(nullptr, std::memory_order_release);
    std::printf("nomoreprofile: SetEnterLeaveFunctionHooks3 failed: 0x%08lx\n",
                static_cast<unsigned long>(hr));
    return hr;
  }

  profilerActive.store(true, std::memory_order_release);

  Log("[*] profiler initialized");
  return S_OK;
}

HRESULT STDMETHODCALLTYPE OfflineProfileUnlockProfiler::InitializeForAttach(
    IUnknown* infoUnknown, void* clientData, UINT clientDataSize) {
  (void)clientData;
  (void)clientDataSize;
  HRESULT hr = Profiler::Initialize(infoUnknown);
  if (FAILED(hr) || pCorProfilerInfo == nullptr)
    return FAILED(hr) ? hr : E_FAIL;
  const DWORD eventMask = COR_PRF_MONITOR_MODULE_LOADS |
                          COR_PRF_MONITOR_JIT_COMPILATION |
                          COR_PRF_DISABLE_INLINING | COR_PRF_ENABLE_REJIT;
  hr = pCorProfilerInfo->SetEventMask2(eventMask, 0);
  if (FAILED(hr)) return hr;
  profilerActive.store(true, std::memory_order_release);
  Log("[+] profiler attached");
  return S_OK;
}

HRESULT STDMETHODCALLTYPE
OfflineProfileUnlockProfiler::ProfilerAttachComplete() {
  HRESULT hr = ApplyTargetRejit();
  if (SUCCEEDED(hr) && target_module_id_ != 0 &&
      target_method_id_ != mdTokenNil) {
    hr = pCorProfilerInfo->RequestReJIT(1, &target_module_id_,
                                        &target_method_id_);
    Log(SUCCEEDED(hr) ? "[+] ReJIT requested" : "[-] ReJIT request failed");
  }
  return hr;
}

HRESULT STDMETHODCALLTYPE OfflineProfileUnlockProfiler::ModuleLoadFinished(
    ModuleID moduleId, HRESULT hrStatus) {
  if (FAILED(hrStatus) || target_module_id_ != 0) return S_OK;
  if (SUCCEEDED(TryFindTarget(moduleId))) {
    HRESULT hr = pCorProfilerInfo->RequestReJIT(1, &target_module_id_,
                                                &target_method_id_);
    Log(SUCCEEDED(hr) ? "[+] ReJIT requested" : "[-] ReJIT request failed");
  }
  return S_OK;
}

HRESULT OfflineProfileUnlockProfiler::ApplyTargetRejit() {
  if (pCorProfilerInfo == nullptr) return E_FAIL;
  ICorProfilerModuleEnum* enumerator = nullptr;
  HRESULT hr = pCorProfilerInfo->EnumModules(&enumerator);
  if (FAILED(hr) || enumerator == nullptr) return FAILED(hr) ? hr : E_FAIL;
  ModuleID modules[32] = {};
  ULONG fetched = 0;
  while (SUCCEEDED(enumerator->Next(32, modules, &fetched)) && fetched != 0) {
    for (ULONG i = 0; i < fetched; ++i) {
      TryFindTarget(modules[i]);
    }
  }
  enumerator->Release();
  Log(target_module_id_ != 0 && target_method_id_ != mdTokenNil
          ? "[+] target method found"
          : "[*] supported target not present in loaded metadata");
  if (target_method_id_ == mdTokenNil) {
    Log("[*] expected current target: "
        "PCL.ProfileUi.CanCreateOtherProfile()");
    Log("[*] expected legacy target: "
        "PCL.ModProfile._GetAvailableProfileSelection(bool)");
  }
  return S_OK;
}

HRESULT OfflineProfileUnlockProfiler::TryFindTarget(ModuleID moduleId) {
  IUnknown* unknown = nullptr;
  if (FAILED(pCorProfilerInfo->GetModuleMetaData(
          moduleId, ofRead, IID_IMetaDataImport, &unknown)) ||
      unknown == nullptr)
    return S_FALSE;

  auto* metadata = reinterpret_cast<IMetaDataImport*>(unknown);
  mdTypeDef type = mdTypeDefNil;
  HRESULT hr = metadata->FindTypeDefByName(L"PCL.ProfileUi", mdTokenNil, &type);
  if (SUCCEEDED(hr)) {
    HCORENUM methods = nullptr;
    mdMethodDef method = mdMethodDefNil;
    ULONG count = 0;
    hr = metadata->EnumMethodsWithName(&methods, type, L"CanCreateOtherProfile",
                                       &method, 1, &count);
    if (SUCCEEDED(hr) && count == 1) {
      target_module_id_ = moduleId;
      target_method_id_ = method;
      target_kind_ = TargetKind::Current;
    } else {
      hr = S_FALSE;
    }
    if (methods != nullptr) metadata->CloseEnum(methods);
  }

  if (target_method_id_ == mdTokenNil) {
    type = mdTypeDefNil;
    hr = metadata->FindTypeDefByName(L"PCL.ModProfile", mdTokenNil, &type);
    if (SUCCEEDED(hr)) {
      HCORENUM methods = nullptr;
      mdMethodDef method = mdMethodDefNil;
      ULONG count = 0;
      hr = metadata->EnumMethodsWithName(
          &methods, type, L"_GetAvailableProfileSelection", &method, 1, &count);
      if (SUCCEEDED(hr) && count == 1) {
        target_module_id_ = moduleId;
        target_method_id_ = method;
        target_kind_ = TargetKind::Legacy;
        Log("[+] legacy PCL profile target detected");
      } else {
        hr = S_FALSE;
      }
      if (methods != nullptr) metadata->CloseEnum(methods);
    }
  }
  metadata->Release();
  return hr;
}

HRESULT OfflineProfileUnlockProfiler::ApplyLegacyRejit(
    ModuleID moduleId, mdMethodDef methodId,
    ICorProfilerFunctionControl* functionControl) {
  ILRewriter rewriter(pCorProfilerInfo, functionControl, moduleId, methodId);
  HRESULT hr = rewriter.Initialize();
  if (FAILED(hr)) return hr;
  hr = rewriter.Import();
  if (FAILED(hr)) return hr;

  ILInstr* first = rewriter.GetILList()->m_pNext;
  ILInstr* forceTrue = rewriter.NewILInstr();
  forceTrue->m_opcode = CEE_LDC_I4_1;
  rewriter.InsertBefore(first, forceTrue);

  ILInstr* storeArgument = rewriter.NewILInstr();
  storeArgument->m_opcode = CEE_STARG_S;
  storeArgument->m_Arg8 = 0;
  rewriter.InsertBefore(first, storeArgument);

  hr = rewriter.Export();
  Log(SUCCEEDED(hr) ? "[+] legacy profile selector forced unrestricted"
                    : "[-] legacy profile selector rewrite failed");
  return hr;
}

HRESULT STDMETHODCALLTYPE OfflineProfileUnlockProfiler::GetReJITParameters(
    ModuleID moduleId, mdMethodDef methodId,
    ICorProfilerFunctionControl* functionControl) {
  if (moduleId != target_module_id_ || methodId != target_method_id_ ||
      functionControl == nullptr)
    return S_OK;

  if (target_kind_ == TargetKind::Legacy)
    return ApplyLegacyRejit(moduleId, methodId, functionControl);

  IUnknown* unknown = nullptr;
  HRESULT hr = pCorProfilerInfo->GetModuleMetaData(moduleId, ofRead | ofWrite,
                                                   IID_IMetaDataEmit, &unknown);
  if (FAILED(hr) || unknown == nullptr) {
    Log("[-] failed to open target metadata for IL logging");
    return FAILED(hr) ? hr : E_FAIL;
  }

  auto* metadata = reinterpret_cast<IMetaDataEmit*>(unknown);
  static const COR_SIGNATURE nativeVoidSignature[] = {IMAGE_CEE_CS_CALLCONV_C,
                                                      0x00, ELEMENT_TYPE_VOID};
  mdSignature callSignature = mdSignatureNil;
  hr = metadata->GetTokenFromSig(nativeVoidSignature,
                                 sizeof(nativeVoidSignature), &callSignature);
  metadata->Release();
  if (FAILED(hr)) {
    Log("[-] failed to create native logging signature");
    return hr;
  }

  // Fat IL body: native enter log, native leave log, then return true.
  BYTE body[44] = {0x03, 0x30, 0x01, 0x00, 0x20, 0x00,
                   0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
  size_t offset = 12;
  const auto appendNativeCall = [&](UINT_PTR address) {
    body[offset++] = 0x21;  // ldc.i8
    const UINT64 pointer = static_cast<UINT64>(address);
    std::memcpy(body + offset, &pointer, sizeof(pointer));
    offset += sizeof(pointer);
    body[offset++] = 0xd3;  // conv.i
    body[offset++] = 0x29;  // calli
    std::memcpy(body + offset, &callSignature, sizeof(callSignature));
    offset += sizeof(callSignature);
  };
  appendNativeCall(reinterpret_cast<UINT_PTR>(&LogTargetEnter));
  appendNativeCall(reinterpret_cast<UINT_PTR>(&LogTargetLeave));
  body[offset++] = 0x17;  // ldc.i4.1
  body[offset++] = 0x2a;  // ret

  hr = functionControl->SetILFunctionBody(static_cast<ULONG>(offset), body);
  Log(SUCCEEDED(hr) ? "[+] ReJIT IL applied" : "[-] ReJIT IL failed");
  return hr;
}

HRESULT STDMETHODCALLTYPE OfflineProfileUnlockProfiler::Shutdown() {
  profilerActive.store(false, std::memory_order_release);
  activeProfiler.store(nullptr, std::memory_order_release);
  return Profiler::Shutdown();
}

bool OfflineProfileUnlockProfiler::IsTarget(FunctionID functionId,
                                            ModuleID* moduleId,
                                            mdMethodDef* methodId) {
  if (functionId == 0 || moduleId == nullptr || methodId == nullptr ||
      pCorProfilerInfo == nullptr)
    return false;

  ClassID classId = 0;
  COR_PRF_FRAME_INFO frameInfo = 0;
  ULONG32 typeArgCount = 0;
  ClassID typeArgs[1] = {};
  mdToken token = mdTokenNil;

  if (FAILED(pCorProfilerInfo->GetFunctionInfo2(functionId, frameInfo, &classId,
                                                moduleId, &token, 1,
                                                &typeArgCount, typeArgs)))
    return false;

  if (token == mdTokenNil || classId == 0) return false;

  const String className = GetClassIDName(classId);
  const String methodName = GetFunctionIDName(functionId);

  if (!(className == String(WCHAR("PCL.ProfileUi")))) return false;
  if (!(methodName == String(WCHAR("CanCreateOtherProfile")))) return false;

  *methodId = static_cast<mdMethodDef>(token);
  return true;
}

HRESULT OfflineProfileUnlockProfiler::ReplaceWithTrue(ModuleID moduleId,
                                                      mdMethodDef methodId) {
  // Fat IL method: header (12 bytes), ldc.i4.1, ret.
  static const BYTE body[] = {
      0x03, 0x30,              // Fat format, header size = 3 dwords
      0x01, 0x00,              // maxstack = 1
      0x02, 0x00, 0x00, 0x00,  // code size = 2
      0x00, 0x00, 0x00, 0x00,  // local signature = nil
      0x17, 0x2A               // ldc.i4.1; ret
  };

  IMethodMalloc* allocator = nullptr;
  HRESULT hr =
      pCorProfilerInfo->GetILFunctionBodyAllocator(moduleId, &allocator);
  if (FAILED(hr) || allocator == nullptr) return FAILED(hr) ? hr : E_FAIL;

  auto* newBody = static_cast<BYTE*>(allocator->Alloc(sizeof(body)));
  if (newBody == nullptr) {
    allocator->Release();
    return E_OUTOFMEMORY;
  }

  std::memcpy(newBody, body, sizeof(body));
  hr = pCorProfilerInfo->SetILFunctionBody(moduleId, methodId, newBody);
  allocator->Release();
  return hr;
}

HRESULT STDMETHODCALLTYPE OfflineProfileUnlockProfiler::JITCompilationStarted(
    FunctionID functionId, BOOL safeToBlock) {
  SHUTDOWNGUARD();

  ModuleID moduleId = 0;
  mdMethodDef methodId = mdTokenNil;
  if (!IsTarget(functionId, &moduleId, &methodId)) return S_OK;

  Log("[+] hook target detected");
  Log("[*] type: PCL.ProfileUi");
  Log("[*] method: CanCreateOtherProfile() -> bool");
  Log("[*] action: replacing IL with return true");

  const HRESULT hr = ReplaceWithTrue(moduleId, methodId);
  if (SUCCEEDED(hr))
    Log("[+] hook applied successfully");
  else
    Log("[-] hook failed");
  return S_OK;
}

}  // namespace nomoreprofile
