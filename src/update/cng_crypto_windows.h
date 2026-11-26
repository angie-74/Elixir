// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#ifndef MOCKTAIL_UPDATE_CNG_CRYPTO_WINDOWS_H_
#define MOCKTAIL_UPDATE_CNG_CRYPTO_WINDOWS_H_

#if !defined(_WIN32)
#error "cng_crypto_windows.h is only for Windows builds"
#endif

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace mocktail::update::cng {

enum class HashAlgorithm { kSha1, kSha256, kSha512 };

std::size_t HashSize(HashAlgorithm algorithm);

class Hash final {
 public:
  explicit Hash(HashAlgorithm algorithm);
  ~Hash();
  Hash(const Hash&) = delete;
  Hash& operator=(const Hash&) = delete;

  bool valid() const { return handle_ != nullptr; }
  bool Update(const void* data, std::size_t size);
  bool Finish(std::vector<unsigned char>* digest);

 private:
  HashAlgorithm algorithm_;
  void* handle_ = nullptr;
};

bool HashBytes(HashAlgorithm algorithm, const void* data, std::size_t size,
               std::vector<unsigned char>* digest);

enum class KeyFamily { kUnsupported, kRsa, kEcdsa, kDsa };

class PublicKey final {
 public:
  PublicKey() = default;
  ~PublicKey();
  PublicKey(const PublicKey&) = delete;
  PublicKey& operator=(const PublicKey&) = delete;
  PublicKey(PublicKey&& other) noexcept;
  PublicKey& operator=(PublicKey&& other) noexcept;

  bool valid() const { return handle_ != nullptr; }
  KeyFamily family() const { return family_; }
  std::uint32_t bits() const { return bits_; }
  const std::vector<unsigned char>& blob() const { return blob_; }
  void* handle() const { return handle_; }

  bool Adopt(void* handle, std::string* error);
  void Reset();

 private:
  void* handle_ = nullptr;
  KeyFamily family_ = KeyFamily::kUnsupported;
  std::uint32_t bits_ = 0;
  std::vector<unsigned char> blob_;
};

bool IsSingleDerElement(const unsigned char* der, std::size_t size);

bool ImportSubjectPublicKeyInfo(const unsigned char* der, std::size_t size,
                                PublicKey* key, std::string* error);

enum class CertificateStatus { kOk, kMalformed, kKeyUnavailable };

CertificateStatus ParseCertificate(const unsigned char* der, std::size_t size,
                                   PublicKey* key);

bool EcdsaDerToP1363(const unsigned char* der, std::size_t size,
                     std::size_t field_bytes, std::vector<unsigned char>* p1363);

bool VerifyRsaPkcs1V15(const PublicKey& key, HashAlgorithm algorithm,
                       const std::vector<unsigned char>& digest,
                       const unsigned char* signature, std::size_t size);
bool VerifyRsaPss(const PublicKey& key, HashAlgorithm algorithm,
                  const std::vector<unsigned char>& digest,
                  const unsigned char* signature, std::size_t size);
bool VerifyEcdsa(const PublicKey& key, const std::vector<unsigned char>& digest,
                 const unsigned char* der_signature, std::size_t size);

bool ConstantTimeEqual(const void* left, const void* right, std::size_t size);

}

#endif
