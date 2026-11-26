// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#ifndef MOCKTAIL_UPDATE_APK_SIGNATURE_CRYPTO_H_
#define MOCKTAIL_UPDATE_APK_SIGNATURE_CRYPTO_H_

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

namespace mocktail::update::apk_crypto {

struct ByteSpan {
  const unsigned char* data = nullptr;
  std::size_t size = 0;
};

enum class DigestAlgorithm { kSha256, kSha512 };

enum class SignatureFamily { kRsaPss, kRsaPkcs1V15, kEcdsa, kDsa };

std::size_t DigestSize(DigestAlgorithm algorithm);

bool DigestParts(DigestAlgorithm algorithm, const std::vector<ByteSpan>& parts,
                 std::vector<unsigned char>* output);

class PublicKey {
 public:
  virtual ~PublicKey() = default;
};

std::unique_ptr<PublicKey> ParseSubjectPublicKeyInfo(ByteSpan encoded);

bool VerifySignature(const PublicKey& key, SignatureFamily family,
                     DigestAlgorithm digest, ByteSpan message,
                     ByteSpan signature, std::string* error);

enum class CertificateStatus { kValid, kMalformed, kKeyMismatch };

CertificateStatus CheckCertificate(ByteSpan encoded,
                                   const PublicKey* expected_key);

bool ConstantTimeEqual(const unsigned char* left, const unsigned char* right,
                       std::size_t size);

}

#endif
