// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0

#include "core/apk_component.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <optional>

#include "core/json.h"
#include "core/paths.h"
#include "core/process.h"
#include "core/sha256.h"
#include "core/util.h"

namespace elixir::launcher {

namespace {

constexpr DWORD kToolTimeoutMs = 10 * 60 * 1000;
constexpr char kGuestAbi[] = "x86_64";

constexpr wchar_t kComponentBroken[] = L"Part of Elixir isn’t working. Reinstall Elixir and try again.";
constexpr wchar_t kNotRoblox[] = L"The file isn’t Roblox, or it’s damaged.";
constexpr wchar_t kNotOfficial[] = L"This isn’t an official Roblox file.";
constexpr wchar_t kInstallFailed[] = L"Try again.";
constexpr wchar_t kNotForPc[] = L"This version of Roblox doesn’t work on PCs.";

struct ToolRun {
  bool started = false;
  bool cancelled = false;
  DWORD exit_code = 0;
  std::string out;
  std::string err;
};

ToolRun RunTool(const ApkComponent& component, std::vector<std::wstring> arguments, std::string* log,
                const std::atomic<bool>* cancel) {
  if (!component.trust_manifest.empty()) {
    arguments.push_back(L"--trust");
    arguments.push_back(component.trust_manifest);
  }
  if (!component.mode_flag.empty()) arguments.insert(arguments.begin(), component.mode_flag);
  const CaptureResult capture =
      RunCaptured(component.path, arguments, ParentDirectory(component.path), {}, cancel, kToolTimeoutMs);
  *log += "$ " + WideToUtf8(FileNameOf(component.path));
  for (const std::wstring& argument : arguments) *log += " " + WideToUtf8(QuoteArgument(argument));
  *log += "\n";
  ToolRun run;
  run.started = capture.started;
  run.cancelled = capture.cancelled;
  run.exit_code = capture.exit_code;
  run.out = capture.stdout_text;
  run.err = capture.stderr_text;
  if (!capture.started) {
    *log += WideToUtf8(capture.error) + "\n\n";
    return run;
  }
  *log += capture.stdout_text;
  if (!capture.stdout_text.empty() && capture.stdout_text.back() != '\n') *log += "\n";
  *log += capture.stderr_text;
  if (!capture.stderr_text.empty() && capture.stderr_text.back() != '\n') *log += "\n";
  if (capture.timed_out) *log += "[stopped after 10 minutes]\n";
  if (capture.cancelled) *log += "[cancelled]\n";
  char code[48];
  std::snprintf(code, sizeof(code), "[exit code %lu]\n\n", static_cast<unsigned long>(capture.exit_code));
  *log += code;
  return run;
}

std::wstring ToolFailure(const ToolRun& run, const wchar_t* message) {
  if (!run.started) return kComponentBroken;
  if (run.cancelled) return L"Cancelled.";
  return message;
}

std::wstring InstallFailure(const std::wstring& cause) {
  return cause.empty() ? std::wstring(kInstallFailed) : cause + L" " + kInstallFailed;
}

std::wstring InstallFailure(DWORD code) { return InstallFailure(ErrorCause(code)); }

std::vector<std::wstring> FindApks(const std::wstring& directory) {
  std::vector<std::wstring> apks;
  WIN32_FIND_DATAW data{};
  HANDLE find = FindFirstFileW(JoinPath(directory, L"*.apk").c_str(), &data);
  if (find == INVALID_HANDLE_VALUE) return apks;
  do {
    if (!(data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) && ExtensionOf(data.cFileName) == L".apk") {
      apks.push_back(JoinPath(directory, data.cFileName));
    }
  } while (FindNextFileW(find, &data));
  FindClose(find);
  std::sort(apks.begin(), apks.end());
  return apks;
}

bool Contains(const std::vector<std::string>& values, std::string_view value) {
  return std::find(values.begin(), values.end(), value) != values.end();
}

void Report(const ApkProgressCallback& progress, int percent, std::string message) {
  if (progress) progress(ApkProgress{percent, std::move(message)});
}

std::wstring SafeSplitName(const std::string& split) {
  std::wstring name;
  for (const char c : split) {
    const bool allowed = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '.' ||
                         c == '_' || c == '-';
    name.push_back(allowed ? static_cast<wchar_t>(c) : L'_');
  }
  return name.empty() ? L"unnamed" : name;
}

}

ApkComponent LocateApkComponent(const std::wstring& exe_dir) {
  ApkComponent component;
  if (!BuiltInProgram().empty()) {
    component.configured = true;
    component.found = true;
    component.path = BuiltInProgram();
    component.mode_flag = kApkToolModeFlag;
    const std::wstring bundled = JoinPath(BundleDirectory(), L"config\\roblox_signing_certificates.json");
    if (!BundleDirectory().empty() && PathIsFile(bundled)) component.trust_manifest = bundled;
    return component;
  }
#if defined(ELIXIR_APK_TOOL_NAME)
  component.configured = true;
  const std::wstring beside = JoinPath(exe_dir, Utf8ToWide(ELIXIR_APK_TOOL_NAME));
  if (PathIsFile(beside)) component.path = beside;
#if defined(ELIXIR_APK_TOOL_BUILD_PATH)
  if (component.path.empty()) {
    std::wstring built = Utf8ToWide(ELIXIR_APK_TOOL_BUILD_PATH);
    std::replace(built.begin(), built.end(), L'/', L'\\');
    if (PathIsFile(built)) component.path = built;
  }
#endif
  component.found = !component.path.empty();
  if (!component.found) component.explanation = L"Part of Elixir is missing. Reinstall Elixir to install Roblox.";
  const std::wstring trust = JoinPath(exe_dir, L"config\\roblox_signing_certificates.json");
  if (PathIsFile(trust)) component.trust_manifest = trust;
#else
  (void)exe_dir;
  component.explanation = L"This version of Elixir can’t install Roblox.";
#endif
  return component;
}

void CleanupApkWorkDirectory(const std::wstring& work_dir) {
  WIN32_FIND_DATAW data{};
  HANDLE find = FindFirstFileW(JoinPath(work_dir, L"*").c_str(), &data);
  if (find != INVALID_HANDLE_VALUE) {
    do {
      const std::wstring name = data.cFileName;
      const std::wstring child = JoinPath(work_dir, name);
      const bool directory = (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
      if (directory && StartsWithI(name, L"xapk-")) {
        DeleteDirectoryTree(child);
      } else if (!directory && StartsWithI(name, L"elixir-archive-")) {
        DeleteFileW(child.c_str());
      }
    } while (FindNextFileW(find, &data));
    FindClose(find);
  }
  RemoveDirectoryW(work_dir.c_str());
}

std::wstring AppInstallDirectory(const std::wstring& apps_dir, const std::string& package, std::int64_t version_code) {
  std::wstring safe;
  for (const char c : package) {
    const bool allowed =
        (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '.' || c == '_';
    safe.push_back(allowed ? static_cast<wchar_t>(c) : L'_');
  }
  while (!safe.empty() && safe.front() == L'.') safe.erase(safe.begin());
  if (safe.empty()) safe = L"unknown";
  return JoinPath(JoinPath(apps_dir, safe), std::to_wstring(version_code));
}

bool ParseInspectReport(std::string_view json_text, ApkFileReport* report, std::string* error) {
  const auto json = Json::Parse(TrimAscii(json_text), error);
  if (!json || !json->is_object()) {
    if (json) *error = "inspect output is not a JSON object";
    return false;
  }
  ApkFileReport result;
  result.path = Utf8ToWide(json->GetString("path"));
  if (const Json* manifest = json->Find("manifest"); manifest != nullptr && manifest->is_object()) {
    result.package = manifest->GetString("package");
    result.version_name = manifest->GetString("version_name");
    const Json* code = manifest->Find("version_code");
    if (code != nullptr) {
      result.version_code =
          code->is_string() ? std::strtoll(code->string_ref().c_str(), nullptr, 10) : code->AsInt(0);
    }
    result.split = manifest->GetString("split");
    result.manifest_error = manifest->GetString("error");
  } else {
    result.manifest_error = "no manifest section in the report";
  }
  if (const Json* signature = json->Find("signature"); signature != nullptr && signature->is_object()) {
    result.signature_verified = signature->GetBool("verified", false);
    if (const Json* certificates = signature->Find("certificate_sha256"); certificates != nullptr) {
      if (certificates->is_array()) {
        for (const Json& certificate : certificates->items()) {
          if (certificate.is_string()) result.certificates.push_back(NormalizeSha256(certificate.string_ref()));
        }
      } else if (certificates->is_string()) {
        result.certificates.push_back(NormalizeSha256(certificates->string_ref()));
      }
    }
    result.trusted = signature->GetBool("trusted", false);
    result.trust_error = signature->GetString("trust_error");
    result.signature_error = signature->GetString("error");
  }
  if (const Json* abis = json->Find("native_abis"); abis != nullptr && abis->is_array()) {
    for (const Json& abi : abis->items()) {
      if (abi.is_string()) result.abis.push_back(abi.string_ref());
    }
  }
  if (const Json* libraries = json->Find("native_libraries"); libraries != nullptr && libraries->is_array()) {
    for (const Json& library : libraries->items()) {
      if (library.GetString("entry") == "lib/x86_64/libroblox.so" && library.GetInt("machine", 0) == 62) {
        result.has_x86_64_engine = true;
        break;
      }
    }
  }
  *report = std::move(result);
  return true;
}

std::vector<std::string> ParseVerifyCertificates(std::string_view verify_stdout) {
  std::vector<std::string> certificates;
  std::size_t start = 0;
  while (start < verify_stdout.size()) {
    std::size_t end = verify_stdout.find('\n', start);
    if (end == std::string_view::npos) end = verify_stdout.size();
    const std::string line = TrimAscii(verify_stdout.substr(start, end - start));
    constexpr std::string_view kKey = "certificate_sha256:";
    if (line.rfind(kKey, 0) == 0) certificates.push_back(NormalizeSha256(TrimAscii(line.substr(kKey.size()))));
    start = end + 1;
  }
  return certificates;
}

std::string PrimaryAbi(const std::vector<std::string>& abis) {
  if (Contains(abis, kGuestAbi)) return kGuestAbi;
  return abis.empty() ? std::string() : abis.front();
}

ApkResult ValidateApk(const ApkComponent& component, const std::wstring& input, const std::wstring& work_dir,
                      const ApkProgressCallback& progress, const std::atomic<bool>* cancel) {
  ApkResult result;
  if (!component.configured || !component.found) {
    result.component_missing = true;
    result.error = component.explanation;
    return result;
  }
  std::vector<std::wstring> apks;
  const std::wstring extension = ExtensionOf(input);
  if (extension == L".xapk" || extension == L".apkm") {
    Report(progress, 3, "Opening the file");
    wchar_t unique[64];
    std::swprintf(unique, 64, L"xapk-%ls-%lu", UtcFileStamp().c_str(), static_cast<unsigned long>(GetTickCount()));
    const std::wstring directory = JoinPath(work_dir, unique);
    if (!EnsureDirectory(directory, &result.error)) return result;
    const CaptureResult unpack = ExtractWithSystemTar(input, directory, cancel, kToolTimeoutMs);
    result.log += "$ tar -x -f " + WideToUtf8(QuoteArgument(input)) + "\n" + unpack.stdout_text + unpack.stderr_text;
    if (!unpack.started) result.log += WideToUtf8(unpack.error) + "\n";
    if (!unpack.started || unpack.exit_code != 0) {
      result.error = L"That file couldn’t be opened. It may be damaged. Choose another file.";
      return result;
    }
    apks = FindApks(directory);
    if (apks.empty()) {
      result.error = L"That file isn’t Roblox. Choose another file.";
      return result;
    }
  } else if (extension == L".apk") {
    apks.push_back(input);
  } else {
    result.error = L"That file isn’t Roblox. Choose another file.";
    return result;
  }
  ApkResult files = ValidateApkFiles(component, apks, progress, cancel);
  files.log = result.log + files.log;
  return files;
}

std::vector<std::wstring> CompanionApkFiles(const std::wstring& chosen) {
  const std::vector<std::wstring> only{chosen};
  const std::wstring name = FileNameOf(chosen);
  std::wstring lower = name;
  for (wchar_t& c : lower) {
    if (c >= L'A' && c <= L'Z') c = static_cast<wchar_t>(c - L'A' + L'a');
  }
  constexpr std::wstring_view kApk = L".apk";
  constexpr std::wstring_view kConfig = L"-config.";
  constexpr std::wstring_view kEngineSplit = L"-config.x86_64";
  if (lower.size() <= kApk.size() || lower.compare(lower.size() - kApk.size(), kApk.size(), kApk) != 0) return only;
  std::wstring base;
  std::wstring split;
  const std::size_t config = lower.find(kConfig);
  if (lower == L"base.apk" || lower == L"split_config.x86_64.apk") {
    base = L"base.apk";
    split = L"split_config.x86_64.apk";
  } else if (config != std::wstring::npos) {
    const std::size_t code = lower.find(L'-', config + kConfig.size());
    if (code == std::wstring::npos) return only;
    base = name.substr(0, config) + name.substr(code);
    split = name.substr(0, config) + std::wstring(kEngineSplit) + name.substr(code);
  } else {
    const std::size_t code = lower.rfind(L'-');
    if (code == std::wstring::npos || code == 0) return only;
    base = name;
    split = name.substr(0, code) + std::wstring(kEngineSplit) + name.substr(code);
  }
  const std::wstring folder = ParentDirectory(chosen);
  const std::wstring base_path = JoinPath(folder, base);
  const std::wstring split_path = JoinPath(folder, split);
  if (!PathIsFile(base_path) || !PathIsFile(split_path)) return only;
  return {base_path, split_path};
}

ApkResult ValidateApkFiles(const ApkComponent& component, const std::vector<std::wstring>& apks,
                           const ApkProgressCallback& progress, const std::atomic<bool>* cancel) {
  ApkResult result;
  if (!component.configured || !component.found) {
    result.component_missing = true;
    result.error = component.explanation;
    return result;
  }
  if (apks.empty()) {
    result.error = L"There’s no file to check.";
    return result;
  }
  std::vector<ApkFileReport> reports;
  for (std::size_t i = 0; i < apks.size(); ++i) {
    Report(progress, 5 + static_cast<int>(45 * i / apks.size()), "Reading the file");
    const ToolRun run = RunTool(component, {L"inspect", apks[i]}, &result.log, cancel);
    if (!run.started || run.exit_code != 0) {
      result.error = ToolFailure(run, kNotRoblox);
      return result;
    }
    ApkFileReport report;
    std::string parse_error;
    if (!ParseInspectReport(run.out, &report, &parse_error)) {
      result.error = kComponentBroken;
      return result;
    }
    report.path = apks[i];
    reports.push_back(std::move(report));
  }

  std::size_t base = reports.size();
  for (std::size_t i = 0; i < reports.size(); ++i) {
    if (reports[i].split.empty() || reports[i].split == "base") {
      if (base != reports.size()) {
        result.error = L"This file contains more than one base APK. Choose a single Roblox bundle.";
        return result;
      }
      base = i;
    }
  }
  if (base == reports.size()) {
    result.error = L"That file is only part of Roblox. Choose the full file.";
    return result;
  }
  const ApkFileReport& base_report = reports[base];
  if (base_report.package != kRobloxPackage) {
    result.error = kNotRoblox;
    return result;
  }
  ApkMetadata& metadata = result.metadata;
  metadata.package = base_report.package;
  metadata.version_code = base_report.version_code;
  metadata.version_name = base_report.version_name;
  metadata.apk_files.push_back(base_report.path);
  for (std::size_t i = 0; i < reports.size(); ++i) {
    const ApkFileReport& report = reports[i];
    if (report.package != metadata.package || report.version_code != metadata.version_code) {
      result.error = L"The parts of this file don’t match. Download it again.";
      return result;
    }
    if (i != base && Contains(metadata.splits, report.split)) {
      result.error = L"This file contains duplicate Roblox splits. Choose another bundle.";
      return result;
    }
    for (const std::string& abi : report.abis) {
      if (!Contains(metadata.abis, abi)) metadata.abis.push_back(abi);
    }
    if (report.has_x86_64_engine) metadata.has_x86_64_engine = true;
    if (i != base) {
      metadata.splits.push_back(report.split);
      metadata.apk_files.push_back(report.path);
    }
  }
  if (!base_report.certificates.empty()) metadata.signer_sha256 = base_report.certificates.front();
  result.reports = reports;

  bool all_verified = true;
  std::vector<std::string> base_signers;
  for (std::size_t i = 0; i < metadata.apk_files.size(); ++i) {
    const std::wstring& apk = metadata.apk_files[i];
    Report(progress, 55 + static_cast<int>(40 * i / metadata.apk_files.size()), "Checking the file");
    const ToolRun run = RunTool(component, {L"verify", apk}, &result.log, cancel);
    if (run.cancelled) {
      result.error = L"Cancelled.";
      return result;
    }
    if (!run.started || run.exit_code != 0) {
      all_verified = false;
      continue;
    }
    auto certificates = ParseVerifyCertificates(run.out);
    if (certificates.empty()) {
      all_verified = false;
    } else if (i == 0) {
      metadata.signer_sha256 = certificates.front();
      base_signers = std::move(certificates);
    } else if (!std::is_permutation(certificates.begin(), certificates.end(), base_signers.begin(), base_signers.end())) {
      all_verified = false;
      result.log += "split APK signing certificates differ from the base APK\n";
    }
  }
  metadata.signature_verified = all_verified;
  if (all_verified) {
    metadata.signer_status = "Official Roblox file";
    result.ok = true;
  } else {
    metadata.signer_status = "Not an official Roblox file";
    result.error = kNotOfficial;
  }
  Report(progress, 100, all_verified ? "Done" : "Check failed");
  return result;
}

ApkResult InstallApk(const ApkComponent& component, const ApkMetadata& validated, const std::wstring& apps_dir,
                     const ApkProgressCallback& progress, const std::atomic<bool>* cancel) {
  ApkResult result;
  result.metadata = validated;
  if (!component.configured || !component.found) {
    result.component_missing = true;
    result.error = component.explanation;
    return result;
  }
  if (!validated.signature_verified || validated.apk_files.empty()) {
    result.error = L"Only an official Roblox file can be installed.";
    return result;
  }
  if (!Contains(validated.abis, kGuestAbi)) {
    result.error = std::wstring(kNotForPc) + L" Choose another file.";
    return result;
  }
  const std::wstring destination = AppInstallDirectory(apps_dir, validated.package, validated.version_code);
  const std::wstring staging = destination + L".installing-" + UtcFileStamp();
  if (!EnsureDirectory(staging, &result.error)) {
    result.error = InstallFailure(result.error);
    return result;
  }
  const auto fail = [&](std::wstring message) {
    result.error = std::move(message);
    DeleteDirectoryTree(staging);
    return result;
  };

  std::vector<std::wstring> copies;
  for (std::size_t i = 0; i < validated.apk_files.size(); ++i) {
    const std::wstring name = i == 0 ? std::wstring(L"base.apk")
                                     : L"split_" + SafeSplitName(i - 1 < validated.splits.size()
                                                                     ? validated.splits[i - 1]
                                                                     : WideToUtf8(FileNameOf(validated.apk_files[i]))) +
                                           L".apk";
    const std::wstring copy = JoinPath(staging, name);
    Report(progress, 5 + static_cast<int>(25 * i / validated.apk_files.size()), "Copying Roblox");
    if (!CopyFileW(validated.apk_files[i].c_str(), copy.c_str(), TRUE)) return fail(InstallFailure(GetLastError()));
    copies.push_back(copy);
  }

  std::wstring native_source;
  for (std::size_t i = 0; i < copies.size(); ++i) {
    Report(progress, 35 + static_cast<int>(20 * i / copies.size()), "Checking Roblox");
    const ToolRun run = RunTool(component, {L"inspect", copies[i]}, &result.log, cancel);
    ApkFileReport report;
    std::string parse_error;
    if (!run.started || run.exit_code != 0) return fail(ToolFailure(run, kInstallFailed));
    if (!ParseInspectReport(run.out, &report, &parse_error)) return fail(kComponentBroken);
    if (report.package != validated.package || report.version_code != validated.version_code) {
      return fail(L"The file changed after it was checked. Try again.");
    }
    if (!report.trusted || !report.signature_verified || !Contains(report.certificates, validated.signer_sha256)) {
      return fail(kNotOfficial);
    }
    if (native_source.empty() && report.has_x86_64_engine) native_source = copies[i];
  }
  if (native_source.empty()) return fail(kNotForPc);

  Report(progress, 60, "Unpacking Roblox");
  {
    const ToolRun run = RunTool(component, {L"extract", native_source, JoinPath(staging, L"lib")}, &result.log, cancel);
    if (!run.started || run.exit_code != 0) return fail(ToolFailure(run, kInstallFailed));
  }
  Report(progress, 70, "Unpacking Roblox’s content");
  {
    const ToolRun run = RunTool(component, {L"extract", copies.front(), JoinPath(staging, L"assets"), L"--prefix",
                                            L"assets/"},
                                &result.log, cancel);
    if (!run.started || run.exit_code != 0) return fail(ToolFailure(run, kInstallFailed));
    if (!PathIsDirectory(JoinPath(staging, L"assets")) && !EnsureDirectory(JoinPath(staging, L"assets"), &result.error)) {
      return fail(InstallFailure(result.error));
    }
  }
  for (const std::wstring& copy : copies) {
    if (copy == native_source || copy == copies.front()) continue;
    Report(progress, 80, "Checking Roblox");
    const ToolRun run = RunTool(component, {L"verify", copy}, &result.log, cancel);
    if (!run.started || run.exit_code != 0) return fail(ToolFailure(run, kInstallFailed));
  }

  Json record = Json::MakeObject();
  record.Set("package", validated.package);
  record.Set("version_code", validated.version_code);
  record.Set("version_name", validated.version_name);
  record.Set("signer_sha256", validated.signer_sha256);
  Json abis = Json::MakeArray();
  for (const std::string& abi : validated.abis) abis.Push(abi);
  record.Set("abis", abis);
  Json split_apks = Json::MakeArray();
  for (std::size_t i = 1; i < copies.size(); ++i) split_apks.Push(WideToUtf8(FileNameOf(copies[i])));
  record.Set("split_apks", std::move(split_apks));
  record.Set("installed_at", UtcIsoTimestamp());
  record.Set("source", WideToUtf8(validated.apk_files.front()));
  std::wstring write_error;
  if (!AtomicWriteFile(JoinPath(staging, L"elixir-install.json"), record.Dump(), AtomicWriteBackup::kNone,
                       &write_error)) {
    return fail(InstallFailure(write_error));
  }

  Report(progress, 92, "Finishing up");
  std::wstring previous;
  if (PathIsDirectory(destination)) {
    previous = destination + L".old-" + UtcFileStamp();
    if (!MoveFileExW(destination.c_str(), previous.c_str(), 0)) return fail(InstallFailure(GetLastError()));
  }
  if (!MoveFileExW(staging.c_str(), destination.c_str(), 0)) {
    const DWORD code = GetLastError();
    if (!previous.empty()) MoveFileExW(previous.c_str(), destination.c_str(), 0);
    return fail(InstallFailure(code));
  }
  if (!previous.empty()) DeleteDirectoryTree(previous);
  result.metadata.install_dir = WideToUtf8(destination);
  result.metadata.apk_files.clear();
  for (const std::wstring& copy : copies) {
    result.metadata.apk_files.push_back(JoinPath(destination, FileNameOf(copy)));
  }
  result.ok = true;
  Report(progress, 100, "Installed");
  return result;
}

ApkResult EnsureInstalledAssets(const ApkComponent& component, const std::wstring& install_dir,
                                const std::atomic<bool>* cancel) {
  ApkResult result;
  const std::wstring assets = JoinPath(install_dir, L"assets");
  if (PathIsDirectory(assets)) {
    result.ok = true;
    return result;
  }
  if (!component.configured || !component.found) {
    result.component_missing = true;
    result.error = component.explanation;
    return result;
  }
  const std::wstring base = JoinPath(install_dir, L"base.apk");
  if (!PathIsFile(base)) {
    result.error = L"Roblox isn’t installed correctly. Install it again.";
    return result;
  }
  const std::wstring staging = assets + L".extracting-" + UtcFileStamp();
  const ToolRun run = RunTool(component, {L"extract", base, staging, L"--prefix", L"assets/"}, &result.log, cancel);
  if (!run.started || run.exit_code != 0) {
    DeleteDirectoryTree(staging);
    result.error = ToolFailure(run, L"Roblox couldn’t be prepared. Install it again.");
    return result;
  }
  if (!PathIsDirectory(staging) && !EnsureDirectory(staging, &result.error)) return result;
  if (!MoveFileExW(staging.c_str(), assets.c_str(), 0)) {
    const DWORD code = GetLastError();
    DeleteDirectoryTree(staging);
    if (PathIsDirectory(assets)) {
      result.ok = true;
      return result;
    }
    result.error = WithErrorCause(L"Roblox couldn’t be prepared.", code);
    return result;
  }
  result.ok = true;
  return result;
}

}
