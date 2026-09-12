#include "cor.h"

// Current Windows SDK headers use ULONG while cor.h exposes uint32_t here.
inline HRESULT CorSigUncompressData(PCCOR_SIGNATURE signature, DWORD length,
                                    ULONG* data, ULONG* dataLength) {
  uint32_t value = 0;
  uint32_t valueLength = 0;
  const HRESULT hr =
      ::CorSigUncompressData(signature, length, &value, &valueLength);
  if (SUCCEEDED(hr)) {
    *data = value;
    *dataLength = valueLength;
  }
  return hr;
}

#include "ilrewriter.cpp"
