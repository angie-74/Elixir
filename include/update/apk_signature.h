// Modified by Elixir.
#ifndef MOCKTAIL_UPDATE_APK_SIGNATURE_H_
#define MOCKTAIL_UPDATE_APK_SIGNATURE_H_

#include <filesystem>
#include <string>
#include <vector>

namespace mocktail::update {

struct ApkSignatureResult {
  std::vector<std::string> certificate_sha256;
  std::string error;

  explicit operator bool() const { return error.empty(); }
};

ApkSignatureResult VerifyApkSignature(const std::filesystem::path& apk_path);

}

#endif
