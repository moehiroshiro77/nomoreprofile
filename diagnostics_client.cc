#include "diagnostics_client.h"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace nomoreprofile {
namespace {
constexpr size_t kHeaderSize = 20;
constexpr uint8_t kProfilerCommandSet = 0x03;
constexpr uint8_t kAttachProfilerCommand = 0x01;

bool WriteAll(HANDLE pipe, const void* data, DWORD size) {
  const auto* bytes = static_cast<const uint8_t*>(data);
  while (size != 0) {
    DWORD written = 0;
    if (!WriteFile(pipe, bytes, size, &written, nullptr) || written == 0)
      return false;
    bytes += written;
    size -= written;
  }
  return true;
}

bool ReadAll(HANDLE pipe, void* data, DWORD size) {
  auto* bytes = static_cast<uint8_t*>(data);
  while (size != 0) {
    DWORD read = 0;
    if (!ReadFile(pipe, bytes, size, &read, nullptr) || read == 0) return false;
    bytes += read;
    size -= read;
  }
  return true;
}

void AppendUint32(std::vector<uint8_t>* payload, uint32_t value) {
  for (int i = 0; i < 4; ++i) payload->push_back((value >> (i * 8)) & 0xff);
}

void AppendString(std::vector<uint8_t>* payload, const wchar_t* value) {
  const size_t length = wcslen(value) + 1;
  AppendUint32(payload, static_cast<uint32_t>(length));
  const auto* bytes = reinterpret_cast<const uint8_t*>(value);
  payload->insert(payload->end(), bytes, bytes + length * sizeof(wchar_t));
}
}  // namespace

bool AttachProfiler(DWORD process_id, const wchar_t* profiler_path,
                    DWORD timeout_ms) {
  wchar_t pipe_name[128] = {};
  swprintf_s(pipe_name, L"\\\\.\\pipe\\dotnet-diagnostic-%lu",
             static_cast<unsigned long>(process_id));
  const ULONGLONG deadline = GetTickCount64() + timeout_ms;
  HANDLE pipe = INVALID_HANDLE_VALUE;
  while (GetTickCount64() < deadline) {
    if (WaitNamedPipeW(pipe_name, 250)) {
      pipe = CreateFileW(pipe_name, GENERIC_READ | GENERIC_WRITE, 0, nullptr,
                         OPEN_EXISTING, 0, nullptr);
      if (pipe != INVALID_HANDLE_VALUE) break;
    }
    Sleep(50);
  }
  if (pipe == INVALID_HANDLE_VALUE) return false;

  std::vector<uint8_t> payload;
  AppendUint32(&payload, timeout_ms / 1000);
  static const uint8_t profiler_guid[16] = {0xe1, 0x2c, 0x2d, 0xf6, 0x64, 0x6b,
                                            0x2f, 0x4c, 0x9c, 0x2c, 0x9d, 0x69,
                                            0x3b, 0xce, 0x8a, 0x31};
  payload.insert(payload.end(), profiler_guid, profiler_guid + 16);
  AppendString(&payload, profiler_path);
  AppendUint32(&payload, 0);

  std::vector<uint8_t> message(kHeaderSize + payload.size(), 0);
  memcpy(message.data(), "DOTNET_IPC_V1", 13);
  const uint16_t size = static_cast<uint16_t>(message.size());
  memcpy(message.data() + 14, &size, sizeof(size));
  message[16] = kProfilerCommandSet;
  message[17] = kAttachProfilerCommand;
  memcpy(message.data() + kHeaderSize, payload.data(), payload.size());

  bool ok = WriteAll(pipe, message.data(), static_cast<DWORD>(message.size()));
  uint8_t response[kHeaderSize] = {};
  if (ok) ok = ReadAll(pipe, response, sizeof(response));
  if (ok && response[17] == 0xff) {
    uint32_t error = 0;
    if (response[14] >= kHeaderSize + sizeof(error))
      ReadAll(pipe, &error, sizeof(error));
    std::fprintf(stderr,
                 "nomoreprofile: runtime rejected AttachProfiler, hr=0x%08lx\n",
                 static_cast<unsigned long>(error));
    ok = false;
  }
  CloseHandle(pipe);
  return ok;
}

}  // namespace nomoreprofile
