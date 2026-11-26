// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#include "update/apk_signature_crypto.h"

#include <memory>
#include <string>
#include <vector>

#include "update/cng_crypto_windows.h"

namespace mocktail::update::apk_crypto {
namespace {

cng::HashAlgorithm ToCng(DigestAlgorithm algorithm) {
  return algorithm == DigestAlgorithm::kSha512 ? cng::HashAlgorithm::kSha512
                                               : cng::HashAlgorithm::kSha256;
}

class CngPublicKey final : public PublicKey {
 public:
  cng::PublicKey key;
};

}

std::size_t DigestSize(DigestAlgorithm algorithm) {
  return cng::HashSize(ToCng(algorithm));
}

bool DigestParts(DigestAlgorithm algorithm, const std::vector<ByteSpan>& parts,
                 std::vector<unsigned char>* output) {
  cng::Hash hash(ToCng(algorithm));
  if (!hash.valid()) return false;
  for (const ByteSpan& part : parts) {
    if (part.size > 0 && !hash.Update(part.data, part.size)) return false;
  }
  return hash.Finish(output) && output->size() == DigestSize(algorithm);
}

std::unique_ptr<PublicKey> ParseSubjectPublicKeyInfo(ByteSpan encoded) {
  auto key = std::make_unique<CngPublicKey>();
  std::string ignored;
  if (!cng::ImportSubjectPublicKeyInfo(encoded.data, encoded.size, &key->key,
                                       &ignored)) {
    return nullptr;
  }
  return key;
}

bool VerifySignature(const PublicKey& key, SignatureFamily family,
                     DigestAlgorithm digest, ByteSpan message,
                     ByteSpan signature, std::string* error) {
  const cng::PublicKey& cng_key = static_cast<const CngPublicKey&>(key).key;
  if (family == SignatureFamily::kDsa) {
    *error =
        "APK signer uses DSA (algorithm 0x0301), which the Windows CNG "
        "backend does not verify";
    return false;
  }
  const cng::KeyFamily expected = family == SignatureFamily::kEcdsa
                                      ? cng::KeyFamily::kEcdsa
                                      : cng::KeyFamily::kRsa;
  if (!cng_key.valid() || cng_key.family() != expected) {
    *error = "APK signer key type does not match its signature algorithm";
    return false;
  }
  std::vector<unsigned char> hash;
  if (!cng::HashBytes(ToCng(digest), message.data, message.size, &hash)) {
    *error = "cannot initialise APK signer verification";
    return false;
  }
  bool verified = false;
  switch (family) {
    case SignatureFamily::kRsaPss:
      verified = cng::VerifyRsaPss(cng_key, ToCng(digest), hash, signature.data,
                                   signature.size);
      break;
    case SignatureFamily::kRsaPkcs1V15:
      verified = cng::VerifyRsaPkcs1V15(cng_key, ToCng(digest), hash,
                                        signature.data, signature.size);
      break;
    case SignatureFamily::kEcdsa:
      verified = cng::VerifyEcdsa(cng_key, hash, signature.data, signature.size);
      break;
    case SignatureFamily::kDsa:
      verified = false;
      break;
  }
  if (!verified) {
    *error = "APK signer signature verification failed";
    return false;
  }
  return true;
}

CertificateStatus CheckCertificate(ByteSpan encoded,
                                   const PublicKey* expected_key) {
  if (expected_key == nullptr) {
    return cng::ParseCertificate(encoded.data, encoded.size, nullptr) ==
                   cng::CertificateStatus::kOk
               ? CertificateStatus::kValid
               : CertificateStatus::kMalformed;
  }
  cng::PublicKey certificate_key;
  switch (cng::ParseCertificate(encoded.data, encoded.size, &certificate_key)) {
    case cng::CertificateStatus::kMalformed:
      return CertificateStatus::kMalformed;
    case cng::CertificateStatus::kKeyUnavailable:
      return CertificateStatus::kKeyMismatch;
    case cng::CertificateStatus::kOk:
      break;
  }
  const cng::PublicKey& signer =
      static_cast<const CngPublicKey*>(expected_key)->key;
  const std::vector<unsigned char>& left = certificate_key.blob();
  const std::vector<unsigned char>& right = signer.blob();
  if (!signer.valid() || left.empty() || left.size() != right.size() ||
      !cng::ConstantTimeEqual(left.data(), right.data(), left.size())) {
    return CertificateStatus::kKeyMismatch;
  }
  return CertificateStatus::kValid;
}

bool ConstantTimeEqual(const unsigned char* left, const unsigned char* right,
                       std::size_t size) {
  return cng::ConstantTimeEqual(left, right, size);
}

}
