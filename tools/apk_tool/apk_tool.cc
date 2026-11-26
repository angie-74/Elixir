// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#include "apk_tool.h"

#include <windows.h>

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include <nlohmann/json.hpp>

#include "compat/elf64_reader.h"
#include "compat/elf_build_id.h"
#include "compat/guest_abi.h"
#include "update/android_manifest.h"
#include "update/apk_bundle.h"
#include "update/apk_signature.h"
#include "update/compatibility_catalog.h"
#include "update/host_file_windows.h"
#include "update/zip_archive.h"

#ifndef ELIXIR_APK_DEFAULT_TRUST_MANIFEST
#define ELIXIR_APK_DEFAULT_TRUST_MANIFEST ""
#endif
#ifndef ELIXIR_APK_DEFAULT_COMPATIBILITY_MANIFEST
#define ELIXIR_APK_DEFAULT_COMPATIBILITY_MANIFEST ""
#endif

namespace elixir::apk_tool {
namespace {

namespace update = mocktail::update;
namespace compat = mocktail::compat;
namespace host_file = mocktail::update::host_file;

constexpr std::size_t kMaximumManifestBytes = 16U * 1024U * 1024U;
constexpr std::size_t kMaximumLibraryBytes = 1024ULL * 1024ULL * 1024ULL;
constexpr std::size_t kMaximumExtractBytes = 4ULL * 1024ULL * 1024ULL * 1024ULL;

constexpr int kSuccess = 0;
constexpr int kFailure = 1;
constexpr int kUsage = 2;

void Usage(std::ostream& errors) {
  errors << "Usage: elixir_apk_tool COMMAND ...\n\n"
            "  verify  APK [--trust TRUST.json | --no-trust-pin]\n"
            "  inspect APK [--trust TRUST.json]\n"
            "  extract APK OUTPUT_DIR [--prefix PREFIX]\n"
            "          [--trust TRUST.json | --no-trust-pin]\n"
            "  prepare WORKSPACE APK [APK...] [--trust TRUST.json]\n"
            "          [--compat CATALOG.json | --version-name NAME\n"
            "           --version-code CODE [--build-id HEX]]\n"
            "  build-id ELF\n\n"
            "TRUST.json defaults to the pinned Roblox signing certificates and\n"
            "CATALOG.json to the compatibility catalog: config\\ next to this\n"
            "executable, else the source tree it was built from.\n";
}

struct Options {
  std::vector<std::string> positional;
  std::map<std::string, std::string> values;
  std::set<std::string> flags;
  std::string error;
};

Options Parse(const std::vector<std::string>& arguments, std::size_t first,
              const std::set<std::string>& valued,
              const std::set<std::string>& switches) {
  Options options;
  for (std::size_t index = first; index < arguments.size(); ++index) {
    const std::string& argument = arguments[index];
    if (argument.rfind("--", 0) != 0) {
      options.positional.push_back(argument);
      continue;
    }
    if (valued.count(argument) != 0) {
      if (index + 1 >= arguments.size()) {
        options.error = argument + " needs a value";
        return options;
      }
      options.values[argument] = arguments[++index];
    } else if (switches.count(argument) != 0) {
      options.flags.insert(argument);
    } else {
      options.error = "unknown option " + argument;
      return options;
    }
  }
  return options;
}

bool ToPath(const std::string& text, std::filesystem::path* path,
            std::ostream& errors) {
  if (!host_file::PathFromUtf8(text, path)) {
    errors << "elixir_apk_tool: path is not valid UTF-8\n";
    return false;
  }
  return true;
}

std::filesystem::path DefaultConfig(const char* name, const char* compiled) {
  std::wstring module(32768, L'\0');
  const DWORD length = GetModuleFileNameW(nullptr, module.data(),
                                          static_cast<DWORD>(module.size()));
  if (length > 0 && length < module.size()) {
    module.resize(length);
    const std::filesystem::path shipped =
        std::filesystem::path(module).parent_path() / "config" / name;
    std::error_code error;
    if (std::filesystem::is_regular_file(shipped, error)) return shipped;
  }
  std::filesystem::path path;
  host_file::PathFromUtf8(compiled, &path);
  return path;
}

std::filesystem::path TrustManifest(const Options& options) {
  const auto value = options.values.find("--trust");
  if (value == options.values.end()) {
    return DefaultConfig("roblox_signing_certificates.json",
                         ELIXIR_APK_DEFAULT_TRUST_MANIFEST);
  }
  std::filesystem::path path;
  host_file::PathFromUtf8(value->second, &path);
  return path;
}

struct TrustCheck {
  bool trusted = false;
  std::vector<std::string> matching;
  std::string error;
};

TrustCheck CheckTrust(const std::vector<std::string>& certificates,
                      const std::filesystem::path& manifest) {
  TrustCheck check;
  const std::vector<std::string> trusted =
      update::LoadTrustedSigningCertificates(manifest, &check.error);
  if (!check.error.empty()) return check;
  for (const std::string& certificate : certificates) {
    if (std::binary_search(trusted.begin(), trusted.end(), certificate)) {
      check.matching.push_back(certificate);
    }
  }
  check.trusted = !check.matching.empty();
  if (!check.trusted) {
    check.error = "APK is not signed by a certificate pinned in " +
                  host_file::Utf8(manifest);
  }
  return check;
}

class PinnedApk final {
 public:
  bool Open(const std::filesystem::path& path, std::ostream& errors) {
    if (file_.Open(path, host_file::ReadOnlyFile::Sharing::kReadOnly) !=
        host_file::OpenStatus::kOk) {
      errors << "elixir_apk_tool: cannot open " << host_file::Utf8(path)
             << " as a regular file\n";
      return false;
    }
    return true;
  }

 private:
  host_file::ReadOnlyFile file_;
};

bool VerifiedAndTrusted(const std::filesystem::path& apk, const Options& options,
                        std::vector<std::string>* certificates,
                        std::ostream& output, std::ostream& errors) {
  const update::ApkSignatureResult signature = update::VerifyApkSignature(apk);
  if (!signature) {
    errors << "elixir_apk_tool: signature: " << signature.error << '\n';
    return false;
  }
  output << "signature: verified (APK Signature Scheme v2/v3)\n";
  for (const std::string& certificate : signature.certificate_sha256) {
    output << "certificate_sha256: " << certificate << '\n';
  }
  *certificates = signature.certificate_sha256;
  if (options.flags.count("--no-trust-pin") != 0) {
    output << "trusted: not checked (--no-trust-pin)\n";
    return true;
  }
  const std::filesystem::path manifest = TrustManifest(options);
  const TrustCheck trust = CheckTrust(signature.certificate_sha256, manifest);
  if (!trust.trusted) {
    errors << "elixir_apk_tool: trust: " << trust.error << '\n';
    return false;
  }
  output << "trusted: yes (" << host_file::Utf8(manifest) << ")\n";
  return true;
}

std::optional<update::AndroidManifestIdentity> ReadIdentity(
    const std::filesystem::path& apk, std::string* error) {
  const update::ZipReadResult manifest =
      update::ReadZipEntry(apk, "AndroidManifest.xml", kMaximumManifestBytes);
  if (!manifest) {
    *error = manifest.error;
    return std::nullopt;
  }
  update::AndroidManifestIdentity identity =
      update::ParseAndroidManifest(manifest.bytes);
  if (!identity) {
    *error = identity.error;
    return std::nullopt;
  }
  return identity;
}

void PrintIdentity(const update::AndroidManifestIdentity& identity,
                   std::ostream& output) {
  output << "package: " << identity.package_name << '\n'
         << "version_name: " << identity.version_name << '\n'
         << "version_code: " << identity.version_code << '\n'
         << "split: "
         << (identity.split_name.empty() ? "base" : identity.split_name)
         << '\n';
}

int Verify(const std::vector<std::string>& arguments, std::ostream& output,
           std::ostream& errors) {
  const Options options =
      Parse(arguments, 1, {"--trust"}, {"--no-trust-pin"});
  if (!options.error.empty() || options.positional.size() != 1 ||
      (options.values.count("--trust") != 0 &&
       options.flags.count("--no-trust-pin") != 0)) {
    if (!options.error.empty()) errors << "elixir_apk_tool: " << options.error << '\n';
    Usage(errors);
    return kUsage;
  }
  std::filesystem::path apk;
  if (!ToPath(options.positional[0], &apk, errors)) return kUsage;
  PinnedApk pinned;
  if (!pinned.Open(apk, errors)) return kFailure;
  std::vector<std::string> certificates;
  if (!VerifiedAndTrusted(apk, options, &certificates, output, errors)) {
    return kFailure;
  }
  std::string error;
  const auto identity = ReadIdentity(apk, &error);
  if (!identity.has_value()) {
    errors << "elixir_apk_tool: manifest: " << error << '\n';
    return kFailure;
  }
  PrintIdentity(*identity, output);
  return kSuccess;
}

int Inspect(const std::vector<std::string>& arguments, std::ostream& output,
            std::ostream& errors) {
  const Options options = Parse(arguments, 1, {"--trust"}, {});
  if (!options.error.empty() || options.positional.size() != 1) {
    if (!options.error.empty()) errors << "elixir_apk_tool: " << options.error << '\n';
    Usage(errors);
    return kUsage;
  }
  std::filesystem::path apk;
  if (!ToPath(options.positional[0], &apk, errors)) return kUsage;
  PinnedApk pinned;
  if (!pinned.Open(apk, errors)) return kFailure;

  const update::ZipListResult listed = update::ListZipEntries(apk);
  if (!listed) {
    errors << "elixir_apk_tool: " << listed.error << '\n';
    return kFailure;
  }
  nlohmann::json report = nlohmann::json::object();
  report["path"] = host_file::Utf8(apk);
  report["entries"] = listed.entries.size();

  std::string error;
  const auto identity = ReadIdentity(apk, &error);
  if (identity.has_value()) {
    report["manifest"] = {
        {"package", identity->package_name},
        {"version_name", identity->version_name},
        {"version_code", identity->version_code},
        {"split", identity->split_name},
    };
  } else {
    report["manifest"] = {{"error", error}};
  }

  const update::ApkSignatureResult signature = update::VerifyApkSignature(apk);
  nlohmann::json signature_report = {{"verified", static_cast<bool>(signature)}};
  if (signature) {
    signature_report["certificate_sha256"] = signature.certificate_sha256;
    const std::filesystem::path manifest = TrustManifest(options);
    const TrustCheck trust = CheckTrust(signature.certificate_sha256, manifest);
    signature_report["trust_manifest"] = host_file::Utf8(manifest);
    signature_report["trusted"] = trust.trusted;
    if (!trust.trusted) signature_report["trust_error"] = trust.error;
  } else {
    signature_report["error"] = signature.error;
  }
  report["signature"] = std::move(signature_report);

  std::map<std::string, std::vector<const update::ZipEntry*>> abis;
  std::size_t assets = 0;
  for (const update::ZipEntry& entry : listed.entries) {
    if (entry.directory) continue;
    if (entry.name.rfind("assets/", 0) == 0) ++assets;
    if (entry.name.rfind("lib/", 0) != 0) continue;
    const std::size_t slash = entry.name.find('/', 4);
    if (slash == std::string::npos || slash == 4 ||
        entry.name.find('/', slash + 1) != std::string::npos) {
      continue;
    }
    abis[entry.name.substr(4, slash - 4)].push_back(&entry);
  }
  report["assets"] = assets;
  nlohmann::json abi_names = nlohmann::json::array();
  for (const auto& [abi, entries] : abis) abi_names.push_back(abi);
  report["native_abis"] = std::move(abi_names);
  nlohmann::json libraries = nlohmann::json::array();
  const auto guest = abis.find(std::string(compat::kGuestAbi));
  if (guest != abis.end()) {
    for (const update::ZipEntry* entry : guest->second) {
      nlohmann::json library = {{"entry", entry->name},
                                {"size", entry->uncompressed_size}};
      const update::ZipReadResult bytes =
          update::ReadZipEntry(apk, entry->name, kMaximumLibraryBytes);
      compat::elf64::Image image;
      std::string parse_error;
      if (!bytes) {
        library["error"] = bytes.error;
      } else if (!image.Parse(bytes.bytes, &parse_error)) {
        library["error"] = parse_error;
      } else {
        library["machine"] = image.machine();
        const std::string build_id = compat::elf64::FindGnuBuildId(image);
        if (build_id.empty()) {
          library["build_id"] = nullptr;
        } else {
          library["build_id"] = build_id;
        }
      }
      libraries.push_back(std::move(library));
    }
  }
  report["native_libraries"] = std::move(libraries);
  output << report.dump(2, ' ', false, nlohmann::json::error_handler_t::replace)
         << '\n';
  return kSuccess;
}

int Extract(const std::vector<std::string>& arguments, std::ostream& output,
            std::ostream& errors) {
  const Options options =
      Parse(arguments, 1, {"--trust", "--prefix"}, {"--no-trust-pin"});
  if (!options.error.empty() || options.positional.size() != 2 ||
      (options.values.count("--trust") != 0 &&
       options.flags.count("--no-trust-pin") != 0)) {
    if (!options.error.empty()) errors << "elixir_apk_tool: " << options.error << '\n';
    Usage(errors);
    return kUsage;
  }
  std::filesystem::path apk;
  std::filesystem::path destination;
  if (!ToPath(options.positional[0], &apk, errors) ||
      !ToPath(options.positional[1], &destination, errors)) {
    return kUsage;
  }
  const auto prefix_value = options.values.find("--prefix");
  const std::string prefix =
      prefix_value != options.values.end()
          ? prefix_value->second
          : "lib/" + std::string(compat::kGuestAbi) + "/";
  if (prefix.empty() || prefix.back() != '/' ||
      !update::IsSafeArchivePath(prefix)) {
    errors << "elixir_apk_tool: --prefix must be a safe directory prefix "
              "ending in '/'\n";
    return kUsage;
  }
  std::error_code filesystem_error;
  if (std::filesystem::exists(destination, filesystem_error) &&
      !std::filesystem::is_empty(destination, filesystem_error)) {
    errors << "elixir_apk_tool: output directory must be empty or absent\n";
    return kFailure;
  }
  PinnedApk pinned;
  if (!pinned.Open(apk, errors)) return kFailure;
  std::vector<std::string> certificates;
  if (!VerifiedAndTrusted(apk, options, &certificates, output, errors)) {
    return kFailure;
  }
  std::size_t files = 0;
  std::string error;
  if (!update::ExtractZipPrefix(apk, prefix, destination, kMaximumExtractBytes,
                                &files, &error)) {
    errors << "elixir_apk_tool: extract: " << error << '\n';
    return kFailure;
  }
  output << "extracted: " << files << " file(s) from " << prefix << '\n';
  std::vector<std::filesystem::path> extracted;
  for (std::filesystem::recursive_directory_iterator iterator(
           destination, filesystem_error),
       end;
       !filesystem_error && iterator != end;
       iterator.increment(filesystem_error)) {
    if (iterator->is_regular_file(filesystem_error)) {
      extracted.push_back(iterator->path());
    }
  }
  std::sort(extracted.begin(), extracted.end());
  for (const std::filesystem::path& file : extracted) {
    const std::string relative =
        host_file::GenericUtf8(file.lexically_relative(destination));
    if (file.extension() == ".so") {
      const compat::BuildIdResult build_id =
          compat::ReadElfBuildId(host_file::Utf8(file));
      output << "file: " << relative << " build_id="
             << (build_id ? build_id.build_id : "none") << '\n';
    } else {
      output << "file: " << relative << '\n';
    }
  }
  return kSuccess;
}

bool ParseUnsigned(const std::string& text, std::uint64_t* value) {
  if (text.empty() || text.size() > 19 ||
      !std::all_of(text.begin(), text.end(),
                   [](char c) { return c >= '0' && c <= '9'; })) {
    return false;
  }
  *value = std::stoull(text);
  return true;
}

int Prepare(const std::vector<std::string>& arguments, std::ostream& output,
            std::ostream& errors) {
  const Options options = Parse(
      arguments, 1,
      {"--trust", "--compat", "--version-name", "--version-code", "--build-id"},
      {});
  const bool explicit_identity = options.values.count("--version-name") != 0 ||
                                 options.values.count("--version-code") != 0 ||
                                 options.values.count("--build-id") != 0;
  if (!options.error.empty() || options.positional.size() < 2 ||
      (explicit_identity && options.values.count("--compat") != 0) ||
      (explicit_identity && (options.values.count("--version-name") == 0 ||
                             options.values.count("--version-code") == 0))) {
    if (!options.error.empty()) errors << "elixir_apk_tool: " << options.error << '\n';
    Usage(errors);
    return kUsage;
  }
  std::filesystem::path workspace;
  if (!ToPath(options.positional[0], &workspace, errors)) return kUsage;
  std::vector<std::filesystem::path> archives;
  for (std::size_t index = 1; index < options.positional.size(); ++index) {
    std::filesystem::path archive;
    if (!ToPath(options.positional[index], &archive, errors)) return kUsage;
    archives.push_back(std::move(archive));
  }
  update::ExpectedPayloadIdentity expected;
  if (explicit_identity) {
    expected.version_name = options.values.at("--version-name");
    if (!ParseUnsigned(options.values.at("--version-code"),
                       &expected.version_code) ||
        expected.version_code == 0) {
      errors << "elixir_apk_tool: --version-code must be a positive integer\n";
      return kUsage;
    }
    const auto build_id = options.values.find("--build-id");
    if (build_id != options.values.end()) {
      if (!compat::IsValidBuildId(build_id->second)) {
        errors << "elixir_apk_tool: --build-id is not a hexadecimal Build ID\n";
        return kUsage;
      }
      std::string lower = build_id->second;
      std::transform(lower.begin(), lower.end(), lower.begin(), [](char c) {
        return static_cast<char>(c >= 'A' && c <= 'F' ? c - 'A' + 'a' : c);
      });
      expected.exact_build_id = lower;
    }
  } else {
    std::string error;
    const auto identity = ReadIdentity(archives.front(), &error);
    if (!identity.has_value()) {
      errors << "elixir_apk_tool: manifest: " << error << '\n';
      return kFailure;
    }
    const auto compat_value = options.values.find("--compat");
    std::filesystem::path catalog_path;
    if (compat_value != options.values.end()) {
      host_file::PathFromUtf8(compat_value->second, &catalog_path);
    } else {
      catalog_path = DefaultConfig("roblox_compatibility.json",
                                   ELIXIR_APK_DEFAULT_COMPATIBILITY_MANIFEST);
    }
    const update::CompatibilityCatalogResult catalog =
        update::LoadCompatibilityCatalog(catalog_path);
    if (!catalog) {
      errors << "elixir_apk_tool: compatibility: " << catalog.error << '\n';
      return kFailure;
    }
    const auto profile = update::FindSupportedProfile(
        catalog.profiles, identity->version_name, identity->version_code);
    if (!profile.has_value()) {
      errors << "elixir_apk_tool: APK is not exact-supported\n";
      return kFailure;
    }
    expected = update::ExactPayloadIdentity(*profile);
  }
  const update::PreparedPayload prepared = update::PreparePayloadFromArchives(
      archives, expected, TrustManifest(options), workspace, "elixir-apk-tool");
  if (!prepared) {
    errors << "elixir_apk_tool: prepare: " << prepared.error << '\n';
    return kFailure;
  }
  output << "prepared: " << host_file::Utf8(prepared.directory) << '\n'
         << "payload_id: " << prepared.payload_id << '\n'
         << "build_id: " << prepared.build_id << '\n';
  return kSuccess;
}

int BuildId(const std::vector<std::string>& arguments, std::ostream& output,
            std::ostream& errors) {
  if (arguments.size() != 2) {
    Usage(errors);
    return kUsage;
  }
  const compat::BuildIdResult result = compat::ReadElfBuildId(arguments[1]);
  if (!result) {
    errors << "elixir_apk_tool: " << result.error << '\n';
    return kFailure;
  }
  output << result.build_id << '\n';
  return kSuccess;
}

}

int Run(const std::vector<std::string>& arguments, std::ostream& output,
        std::ostream& errors) {
  if (arguments.empty()) {
    Usage(errors);
    return kUsage;
  }
  const std::string& command = arguments[0];
  if (command == "verify") return Verify(arguments, output, errors);
  if (command == "inspect") return Inspect(arguments, output, errors);
  if (command == "extract") return Extract(arguments, output, errors);
  if (command == "prepare") return Prepare(arguments, output, errors);
  if (command == "build-id") return BuildId(arguments, output, errors);
  if (command == "--help" || command == "-h" || command == "help") {
    Usage(output);
    return kSuccess;
  }
  errors << "elixir_apk_tool: unknown command " << command << '\n';
  Usage(errors);
  return kUsage;
}

}
