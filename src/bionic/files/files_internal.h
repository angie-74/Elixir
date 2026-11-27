// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#ifndef ELIXIR_BIONIC_FILES_FILES_INTERNAL_H_
#define ELIXIR_BIONIC_FILES_FILES_INTERNAL_H_

#include <windows.h>

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <new>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "bionic/files/files.h"
#include "bionic/runtime.h"
#include "bionic/vfs.h"
#include "platform/windows/guest_tls.h"

namespace elixir::bionic::files {

inline constexpr std::int64_t kUTIME_NOW = (1LL << 30) - 1;
inline constexpr std::int64_t kUTIME_OMIT = (1LL << 30) - 2;
inline constexpr int kLOCK_SH = 1, kLOCK_EX = 2, kLOCK_NB = 4, kLOCK_UN = 8;
inline constexpr int kFALLOC_FL_KEEP_SIZE = 0x01, kFALLOC_FL_PUNCH_HOLE = 0x02;
inline constexpr std::uint64_t kST_RDONLY = 1, kST_NOSUID = 2, kST_NODEV = 4;
inline constexpr int kFSETLOCKING_QUERY = 0, kFSETLOCKING_INTERNAL = 1, kFSETLOCKING_BYCALLER = 2;
inline constexpr std::uint32_t kWEOF = 0xffffffffu;
inline constexpr int kRENAME_NOREPLACE = 1, kRENAME_EXCHANGE = 2, kRENAME_WHITEOUT = 4;
inline constexpr std::uint32_t kSTATX_BASIC_STATS = 0x7ff, kSTATX_BTIME = 0x800;
inline constexpr int kAT_STATX_SYNC_MASK = 0x6000;
inline constexpr unsigned kCLOSE_RANGE_UNSHARE = 1u << 1, kCLOSE_RANGE_CLOEXEC = 1u << 2;
inline constexpr int kRWF_HIPRI = 0x01, kRWF_DSYNC = 0x02, kRWF_SYNC = 0x04, kRWF_NOWAIT = 0x08,
                     kRWF_APPEND = 0x10, kRWF_SUPPORTED = 0x1f;

inline constexpr std::uint32_t kGuestUid = 10100;
inline constexpr std::uint32_t kGuestGid = 10100;

struct GuestFlock {
  std::int16_t l_type;
  std::int16_t l_whence;
  std::int64_t l_start;
  std::int64_t l_len;
  std::int32_t l_pid;
};
static_assert(sizeof(GuestFlock) == 32, "struct flock");

struct GuestUtimbuf {
  abi::g_time actime;
  abi::g_time modtime;
};

struct GuestStatxTimestamp {
  std::int64_t tv_sec;
  std::uint32_t tv_nsec;
  std::int32_t reserved;
};
struct GuestStatx {
  std::uint32_t stx_mask;
  std::uint32_t stx_blksize;
  std::uint64_t stx_attributes;
  std::uint32_t stx_nlink;
  std::uint32_t stx_uid;
  std::uint32_t stx_gid;
  std::uint16_t stx_mode;
  std::uint16_t spare0;
  std::uint64_t stx_ino;
  std::uint64_t stx_size;
  std::uint64_t stx_blocks;
  std::uint64_t stx_attributes_mask;
  GuestStatxTimestamp stx_atime, stx_btime, stx_ctime, stx_mtime;
  std::uint32_t stx_rdev_major, stx_rdev_minor, stx_dev_major, stx_dev_minor;
  std::uint64_t stx_mnt_id;
  std::uint64_t spare2[13];
};
static_assert(sizeof(GuestStatx) == 256, "struct statx");
static_assert(offsetof(GuestStatx, stx_atime) == 64, "statx atime");
static_assert(offsetof(GuestStatx, stx_rdev_major) == 128, "statx rdev");

template <typename R, typename F>
R Guarded(R failure, F&& body) noexcept {
  const platform::GuestThreadResume resume;
  try {
    return body();
  } catch (const std::bad_alloc&) {
    SetGuestErrno(guest::kENOMEM);
  } catch (...) {
    SetGuestErrno(guest::kEIO);
  }
  return failure;
}

[[noreturn]] void FortifyFatal(const char* function, const char* what) noexcept;

std::wstring Utf8ToWide(std::string_view text);
std::string WideToUtf8(std::wstring_view text);

void AppendEscapedComponent(std::wstring* out, std::string_view component);
std::string UnescapeHostName(std::wstring_view name);

void FileTimeToTimespec(std::int64_t filetime, guest::Timespec* out) noexcept;
std::int64_t TimespecToFileTime(std::int64_t sec, std::int64_t nsec) noexcept;
std::int64_t NowFileTime() noexcept;

constexpr std::uint64_t MakeDev(std::uint64_t major, std::uint64_t minor) {
  return ((major & 0xfffff000ull) << 32) | ((major & 0xfffull) << 8) | ((minor & 0xffffff00ull) << 12) |
         (minor & 0xffull);
}

bool FillRandom(void* buffer, std::size_t length) noexcept;
std::uint64_t HashString(std::string_view text) noexcept;

int MissingPathErrno(const std::wstring& host_path);
int PathErrnoFromWin32(DWORD error, const std::wstring& host_path);

struct HostFileInfo {
  DWORD attributes = 0;
  DWORD reparse_tag = 0;
  std::uint64_t volume_serial = 0;
  std::uint64_t file_id = 0;
  std::uint32_t links = 1;
  std::int64_t size = 0;
  std::int64_t allocation = 0;
  std::int64_t creation_time = 0;
  std::int64_t access_time = 0;
  std::int64_t write_time = 0;
  std::int64_t change_time = 0;
};
bool QueryHandleInfo(HANDLE handle, HostFileInfo* info) noexcept;
bool IsSymlinkTag(DWORD tag) noexcept;

enum class NodeKind : std::uint8_t { kHost, kSynthetic, kVirtualDir };

struct Resolved {
  NodeKind kind = NodeKind::kHost;
  std::string guest;
  bool host_absolute = false;
  std::wstring host;
  bool read_only = false;
  bool trailing_slash = false;
  bool last_is_dot = false;
  bool mount_root = false;
};

bool Resolve(std::string_view path, Resolved* out, int* err);
bool ResolveFrom(std::string_view base, std::string_view path, Resolved* out, int* err);
bool ResolveAt(int dirfd, const char* path, Resolved* out, int* err);

enum class ContentCacheOperation { kOpen, kRead, kWrite, kRename, kMap };
void RecordContentCacheIoFailure(ContentCacheOperation operation, int guest_errno,
                                 std::string_view path, std::string_view other_path = {}) noexcept;
void RecordContentCacheOpenResult(std::string_view path, int guest_errno) noexcept;
void RecordAppStorageQuery(const Resolved& path, const char* operation, int guest_errno) noexcept;

const char* ProcResourceLabel(std::string_view guest_path) noexcept;
void RecordProcOpenResult(const char* resource, int guest_errno, std::string_view requested_guest) noexcept;
void RecordMissingSystemPath(std::string_view guest_path, const char* operation, int guest_errno) noexcept;

std::string GuestCwd();
void SetGuestCwd(std::string path);

std::vector<std::string> MountChildren(const std::string& guest_dir);
bool HasMountBelow(const std::string& guest);

std::wstring HostTempDirectory();

std::vector<std::pair<std::string, bool>> MountList();

std::wstring StripLongPrefixPath(std::wstring path);

enum class SynthType : std::uint8_t { kNone, kDir, kFile, kNull, kZero, kRandom, kLink, kFdAlias };

using ProcSample = std::string (*)();

struct SynthNode {
  SynthType type = SynthType::kNone;
  std::uint32_t mode = 0;
  std::uint64_t rdev = 0;
  std::uint64_t ino = 0;
  std::uint64_t dev = 0;
  std::int64_t size = 0;
  int alias_fd = -1;
  std::string link_target;
  ProcSample sample = nullptr;
};

bool SynthLookup(const std::string& guest, SynthNode* node);
std::string SynthContent(const std::string& guest);
std::vector<std::pair<std::string, std::uint8_t>> SynthList(const std::string& guest);
std::uint64_t SynthFsType(const std::string& guest);
bool FollowSynthetic(Resolved* r, SynthNode* node, int* err, std::string* original_guest = nullptr);

int DeleteHostPath(const std::wstring& host, bool directory);
bool ReadHostLink(const std::wstring& host, std::string* target, int* err);

int OpenAt(int dirfd, const char* path, int flags, std::uint32_t mode);

enum class Provider : std::uint8_t { kHandle, kZero, kRandom, kMemory, kVirtualDir };

struct Description {
  Description() = default;
  Description(const Description&) = delete;
  Description& operator=(const Description&) = delete;
  ~Description();

  vfs::FdKind kind = vfs::FdKind::kFile;
  Provider provider = Provider::kHandle;
  HANDLE handle = nullptr;
  std::atomic<int> status_flags{0};
  bool use_os_pointer = false;
  bool read_only_mount = false;
  bool synthetic_dir = false;
  std::mutex lock;
  std::int64_t position = 0;
  std::string memory;
  ProcSample sample = nullptr;
  std::string guest_path;
  std::wstring host_path;
  std::string console_out_carry;
  std::string console_in_pending;
  const FdOps* ops = nullptr;
};
using DescriptionPtr = std::shared_ptr<Description>;

void RecordProcReadResult(const Description& description, bool positional, std::optional<std::size_t> requested,
                          std::int64_t result) noexcept;

inline int AccessMode(const Description& d) { return d.status_flags.load() & guest::kO_ACCMODE; }
inline bool IsPathOnly(const Description& d) { return (d.status_flags.load() & guest::kO_PATH) != 0; }
inline bool IsDirectoryDescription(const Description& d) {
  return d.kind == vfs::FdKind::kDirectory || d.provider == Provider::kVirtualDir;
}

inline constexpr int kMaxFds = 32768;

DescriptionPtr GetDescription(int fd);
DescriptionPtr FindDescription(int fd);
int InstallDescription(DescriptionPtr description, bool cloexec, int min_fd = 0);
int InstallDescriptionAt(DescriptionPtr description, int fd, bool cloexec);
int CloseFd(int fd);
bool GetCloexec(int fd, bool* cloexec);
bool SetCloexec(int fd, bool cloexec);
std::vector<int> OpenFds();
std::string FdTargetPath(int fd);

std::int64_t DescRead(Description& d, void* buffer, std::size_t count);
std::int64_t DescWrite(Description& d, const void* buffer, std::size_t count);
std::int64_t DescPread(Description& d, void* buffer, std::size_t count, std::int64_t offset);
std::int64_t DescPwrite(Description& d, const void* buffer, std::size_t count, std::int64_t offset);
std::int64_t DescSeek(Description& d, std::int64_t offset, int whence);
void DescSetStatusFlags(Description& d, int flags);
std::int64_t DescAvailable(Description& d);
bool DescIsSeekable(const Description& d);

DescriptionPtr MakeHandleDescription(HANDLE handle, vfs::FdKind kind, int flags);
DescriptionPtr OpenSyntheticDescription(const std::string& guest, const SynthNode& node, int flags, int* err);

bool DescStat(Description& d, guest::Stat* st, int* err);

std::int64_t DescWriteAppend(Description& d, const void* buffer, std::size_t count, std::int64_t offset);

bool ExchangeFdOwnerTag(int fd, std::uint64_t expected, std::uint64_t desired, std::uint64_t* actual);
std::uint64_t FdOwnerTag(int fd);

const FdOps* HookOps(const Description& d) noexcept;
std::int64_t HookedRead(Description& d, const FdOps& ops, void* buffer, std::size_t count);
std::int64_t HookedWrite(Description& d, const FdOps& ops, const void* buffer, std::size_t count);
int HookedBytesAvailable(Description& d, const FdOps& ops, int* available);
void HookedSetNonBlocking(Description& d, const FdOps& ops, bool nonblocking) noexcept;
std::uint32_t HookedPoll(Description& d, const FdOps& ops, std::uint32_t events) noexcept;
void HookedClose(Description& d, const FdOps& ops) noexcept;

int FdsanClose(int fd, std::uint64_t expected_tag);

void InotifyNoteGuestWrite(const Description& d) noexcept;
void InotifyNoteGuestClose(const std::wstring& host_path, bool directory, bool writable) noexcept;

void FillStatFromInfo(const HostFileInfo& info, std::string_view name_hint, guest::Stat* st) noexcept;
void FillSynthStat(const SynthNode& node, guest::Stat* st) noexcept;
void FillDirStat(const std::string& guest, guest::Stat* st) noexcept;
bool StatResolved(const Resolved& r, bool follow, guest::Stat* st, int* err);

HANDLE OpenForAttributes(const std::wstring& host, bool follow, DWORD extra_access = 0) noexcept;

std::uint32_t CurrentUmask() noexcept;

void RegisterFdExports(ExportTable& table);
void RegisterMetadataExports(ExportTable& table);
void RegisterPathExports(ExportTable& table);
void RegisterDirExports(ExportTable& table);
void RegisterExtraFileExports(ExportTable& table);
void RegisterStdioCoreExports(ExportTable& table);
void RegisterStdioMemExports(ExportTable& table);
void RegisterStdioWideExports(ExportTable& table);
void RegisterStdioFormatExports(ExportTable& table);
void RegisterFdsanExports(ExportTable& table);
void RegisterFileExtExports(ExportTable& table);
void RegisterInotifyExports(ExportTable& table);
void RegisterStdioUnlockedExports(ExportTable& table);

}

#endif
