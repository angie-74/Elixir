// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#ifndef ELIXIR_LAUNCHER_CORE_APK_COMPONENT_H_
#define ELIXIR_LAUNCHER_CORE_APK_COMPONENT_H_

#include <atomic>
#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace elixir::launcher {

struct ApkComponent {
  bool configured = false;
  bool found = false;
  std::wstring path;
  std::wstring mode_flag;
  std::wstring trust_manifest;
  std::wstring explanation;
};

ApkComponent LocateApkComponent(const std::wstring& exe_dir);

struct ApkFileReport {
  std::wstring path;
  std::string package;
  std::int64_t version_code = 0;
  std::string version_name;
  std::string split;
  std::vector<std::string> abis;
  bool has_x86_64_engine = false;
  bool signature_verified = false;
  std::vector<std::string> certificates;
  bool trusted = false;
  std::string manifest_error;
  std::string signature_error;
  std::string trust_error;
};

struct ApkMetadata {
  std::string package;
  std::int64_t version_code = 0;
  std::string version_name;
  std::vector<std::string> abis;
  bool has_x86_64_engine = false;
  std::vector<std::string> splits;
  std::string signer_sha256;
  std::string signer_status;
  bool signature_verified = false;
  std::vector<std::wstring> apk_files;
  std::string install_dir;
};

struct ApkProgress {
  int percent = -1;
  std::string message;
};

struct ApkResult {
  bool ok = false;
  bool component_missing = false;
  ApkMetadata metadata;
  std::vector<ApkFileReport> reports;
  std::string log;
  std::wstring error;
};

using ApkProgressCallback = std::function<void(const ApkProgress&)>;

ApkResult ValidateApk(const ApkComponent& component, const std::wstring& input, const std::wstring& work_dir,
                      const ApkProgressCallback& progress, const std::atomic<bool>* cancel);

ApkResult ValidateApkFiles(const ApkComponent& component, const std::vector<std::wstring>& apks,
                           const ApkProgressCallback& progress, const std::atomic<bool>* cancel);

std::vector<std::wstring> CompanionApkFiles(const std::wstring& chosen);

ApkResult InstallApk(const ApkComponent& component, const ApkMetadata& validated, const std::wstring& apps_dir,
                     const ApkProgressCallback& progress, const std::atomic<bool>* cancel);

ApkResult EnsureInstalledAssets(const ApkComponent& component, const std::wstring& install_dir,
                                const std::atomic<bool>* cancel = nullptr);

void CleanupApkWorkDirectory(const std::wstring& work_dir);

std::wstring AppInstallDirectory(const std::wstring& apps_dir, const std::string& package, std::int64_t version_code);

bool ParseInspectReport(std::string_view json_text, ApkFileReport* report, std::string* error);
std::vector<std::string> ParseVerifyCertificates(std::string_view verify_stdout);
std::string PrimaryAbi(const std::vector<std::string>& abis);

}

#endif
