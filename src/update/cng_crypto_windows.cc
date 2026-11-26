// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#include "update/cng_crypto_windows.h"

#include <windows.h>
#include <bcrypt.h>
#include <wincrypt.h>

#include <cstring>
#include <limits>
#include <string>
#include <utility>
#include <vector>

namespace mocktail::update::cng {
namespace {

constexpr NTSTATUS kStatusSuccess = 0;

struct Providers {
  BCRYPT_ALG_HANDLE sha1 = nullptr;
  BCRYPT_ALG_HANDLE sha256 = nullptr;
  BCRYPT_ALG_HANDLE sha512 = nullptr;

  Providers() {
    if (!BCRYPT_SUCCESS(BCryptOpenAlgorithmProvider(&sha1, BCRYPT_SHA1_ALGORITHM,
                                                    nullptr, 0))) {
      sha1 = nullptr;
    }
    if (!BCRYPT_SUCCESS(BCryptOpenAlgorithmProvider(
            &sha256, BCRYPT_SHA256_ALGORITHM, nullptr, 0))) {
      sha256 = nullptr;
    }
    if (!BCRYPT_SUCCESS(BCryptOpenAlgorithmProvider(
            &sha512, BCRYPT_SHA512_ALGORITHM, nullptr, 0))) {
      sha512 = nullptr;
    }
  }
};

BCRYPT_ALG_HANDLE Provider(HashAlgorithm algorithm) {
  static const Providers providers;
  switch (algorithm) {
    case HashAlgorithm::kSha1:
      return providers.sha1;
    case HashAlgorithm::kSha256:
      return providers.sha256;
    case HashAlgorithm::kSha512:
      return providers.sha512;
  }
  return nullptr;
}

LPCWSTR AlgorithmName(HashAlgorithm algorithm) {
  switch (algorithm) {
    case HashAlgorithm::kSha1:
      return BCRYPT_SHA1_ALGORITHM;
    case HashAlgorithm::kSha256:
      return BCRYPT_SHA256_ALGORITHM;
    case HashAlgorithm::kSha512:
      return BCRYPT_SHA512_ALGORITHM;
  }
  return nullptr;
}

bool ReadDerHeader(const unsigned char* der, std::size_t size,
                   std::size_t offset, unsigned char* tag,
                   std::size_t* content_offset, std::size_t* content_length) {
  if (der == nullptr || offset >= size || size - offset < 2) return false;
  const unsigned char identifier = der[offset];
  if ((identifier & 0x1fU) == 0x1fU) return false;
  std::size_t cursor = offset + 1;
  const unsigned char first = der[cursor++];
  std::size_t length = 0;
  if (first < 0x80U) {
    length = first;
  } else {
    const std::size_t count = first & 0x7fU;
    if (count == 0 || count > sizeof(std::size_t) || size - cursor < count ||
        der[cursor] == 0) {
      return false;
    }
    for (std::size_t index = 0; index < count; ++index) {
      length = (length << 8U) | der[cursor++];
    }
    if (length < 0x80U) return false;
  }
  if (length > size - cursor) return false;
  *tag = identifier;
  *content_offset = cursor;
  *content_length = length;
  return true;
}

class LocalBuffer final {
 public:
  ~LocalBuffer() {
    if (pointer != nullptr) LocalFree(pointer);
  }
  void* pointer = nullptr;
};

}

std::size_t HashSize(HashAlgorithm algorithm) {
  switch (algorithm) {
    case HashAlgorithm::kSha1:
      return 20;
    case HashAlgorithm::kSha256:
      return 32;
    case HashAlgorithm::kSha512:
      return 64;
  }
  return 0;
}

Hash::Hash(HashAlgorithm algorithm) : algorithm_(algorithm) {
  BCRYPT_ALG_HANDLE provider = Provider(algorithm);
  BCRYPT_HASH_HANDLE handle = nullptr;
  if (provider != nullptr &&
      BCRYPT_SUCCESS(
          BCryptCreateHash(provider, &handle, nullptr, 0, nullptr, 0, 0))) {
    handle_ = handle;
  }
}

Hash::~Hash() {
  if (handle_ != nullptr) BCryptDestroyHash(static_cast<BCRYPT_HASH_HANDLE>(handle_));
}

bool Hash::Update(const void* data, std::size_t size) {
  if (handle_ == nullptr) return false;
  if (size == 0) return true;
  if (data == nullptr) return false;
  const auto* bytes = static_cast<const unsigned char*>(data);
  while (size > 0) {
    const ULONG chunk = static_cast<ULONG>(
        size > (1U << 30U) ? (1U << 30U) : size);
    if (!BCRYPT_SUCCESS(BCryptHashData(static_cast<BCRYPT_HASH_HANDLE>(handle_),
                                       const_cast<PUCHAR>(bytes), chunk, 0))) {
      return false;
    }
    bytes += chunk;
    size -= chunk;
  }
  return true;
}

bool Hash::Finish(std::vector<unsigned char>* digest) {
  if (handle_ == nullptr) return false;
  digest->assign(HashSize(algorithm_), 0);
  const NTSTATUS status =
      BCryptFinishHash(static_cast<BCRYPT_HASH_HANDLE>(handle_), digest->data(),
                       static_cast<ULONG>(digest->size()), 0);
  BCryptDestroyHash(static_cast<BCRYPT_HASH_HANDLE>(handle_));
  handle_ = nullptr;
  if (!BCRYPT_SUCCESS(status)) {
    digest->clear();
    return false;
  }
  return true;
}

bool HashBytes(HashAlgorithm algorithm, const void* data, std::size_t size,
               std::vector<unsigned char>* digest) {
  Hash hash(algorithm);
  return hash.valid() && hash.Update(data, size) && hash.Finish(digest);
}

PublicKey::~PublicKey() { Reset(); }

PublicKey::PublicKey(PublicKey&& other) noexcept { *this = std::move(other); }

PublicKey& PublicKey::operator=(PublicKey&& other) noexcept {
  if (this != &other) {
    Reset();
    handle_ = other.handle_;
    family_ = other.family_;
    bits_ = other.bits_;
    blob_ = std::move(other.blob_);
    other.handle_ = nullptr;
    other.family_ = KeyFamily::kUnsupported;
    other.bits_ = 0;
    other.blob_.clear();
  }
  return *this;
}

void PublicKey::Reset() {
  if (handle_ != nullptr) BCryptDestroyKey(static_cast<BCRYPT_KEY_HANDLE>(handle_));
  handle_ = nullptr;
  family_ = KeyFamily::kUnsupported;
  bits_ = 0;
  blob_.clear();
}

bool PublicKey::Adopt(void* handle, std::string* error) {
  Reset();
  if (handle == nullptr) {
    *error = "no public key handle";
    return false;
  }
  handle_ = handle;
  BCRYPT_KEY_HANDLE key = static_cast<BCRYPT_KEY_HANDLE>(handle);
  ULONG size = 0;
  if (!BCRYPT_SUCCESS(BCryptExportKey(key, nullptr, BCRYPT_PUBLIC_KEY_BLOB,
                                      nullptr, 0, &size, 0)) ||
      size < sizeof(ULONG)) {
    *error = "cannot export public key";
    Reset();
    return false;
  }
  blob_.assign(size, 0);
  if (!BCRYPT_SUCCESS(BCryptExportKey(key, nullptr, BCRYPT_PUBLIC_KEY_BLOB,
                                      blob_.data(), size, &size, 0)) ||
      size > blob_.size()) {
    *error = "cannot export public key";
    Reset();
    return false;
  }
  blob_.resize(size);
  ULONG magic = 0;
  std::memcpy(&magic, blob_.data(), sizeof(magic));
  switch (magic) {
    case BCRYPT_RSAPUBLIC_MAGIC: {
      BCRYPT_RSAKEY_BLOB header = {};
      if (blob_.size() < sizeof(header)) break;
      std::memcpy(&header, blob_.data(), sizeof(header));
      if (header.BitLength == 0 ||
          blob_.size() - sizeof(header) <
              static_cast<std::size_t>(header.cbPublicExp) + header.cbModulus) {
        break;
      }
      family_ = KeyFamily::kRsa;
      bits_ = header.BitLength;
      break;
    }
    case BCRYPT_ECDSA_PUBLIC_P256_MAGIC:
    case BCRYPT_ECDSA_PUBLIC_P384_MAGIC:
    case BCRYPT_ECDSA_PUBLIC_P521_MAGIC: {
      BCRYPT_ECCKEY_BLOB header = {};
      if (blob_.size() < sizeof(header)) break;
      std::memcpy(&header, blob_.data(), sizeof(header));
      if (blob_.size() - sizeof(header) < 2U * static_cast<std::size_t>(header.cbKey)) {
        break;
      }
      family_ = KeyFamily::kEcdsa;
      bits_ = magic == BCRYPT_ECDSA_PUBLIC_P256_MAGIC
                  ? 256U
                  : (magic == BCRYPT_ECDSA_PUBLIC_P384_MAGIC ? 384U : 521U);
      if (header.cbKey != (bits_ + 7U) / 8U) {
        family_ = KeyFamily::kUnsupported;
        bits_ = 0;
      }
      break;
    }
    case BCRYPT_DSA_PUBLIC_MAGIC:
    case BCRYPT_DSA_PUBLIC_MAGIC_V2:
      family_ = KeyFamily::kDsa;
      break;
    default:
      break;
  }
  return true;
}

bool IsSingleDerElement(const unsigned char* der, std::size_t size) {
  unsigned char tag = 0;
  std::size_t content = 0;
  std::size_t length = 0;
  return ReadDerHeader(der, size, 0, &tag, &content, &length) &&
         content + length == size;
}

bool ImportSubjectPublicKeyInfo(const unsigned char* der, std::size_t size,
                                PublicKey* key, std::string* error) {
  key->Reset();
  if (size == 0 || size > std::numeric_limits<DWORD>::max() ||
      !IsSingleDerElement(der, size) || der[0] != 0x30U) {
    *error = "public key is not exactly one DER SubjectPublicKeyInfo";
    return false;
  }
  LocalBuffer decoded;
  DWORD decoded_size = 0;
  if (CryptDecodeObjectEx(X509_ASN_ENCODING, X509_PUBLIC_KEY_INFO, der,
                          static_cast<DWORD>(size), CRYPT_DECODE_ALLOC_FLAG,
                          nullptr, &decoded.pointer, &decoded_size) == 0 ||
      decoded.pointer == nullptr) {
    *error = "cannot decode SubjectPublicKeyInfo";
    return false;
  }
  BCRYPT_KEY_HANDLE handle = nullptr;
  if (CryptImportPublicKeyInfoEx2(
          X509_ASN_ENCODING, static_cast<PCERT_PUBLIC_KEY_INFO>(decoded.pointer),
          CRYPT_OID_INFO_PUBKEY_SIGN_KEY_FLAG, nullptr, &handle) == 0) {
    *error = "cannot import SubjectPublicKeyInfo";
    return false;
  }
  return key->Adopt(handle, error);
}

CertificateStatus ParseCertificate(const unsigned char* der, std::size_t size,
                                   PublicKey* key) {
  if (key != nullptr) key->Reset();
  if (size == 0 || size > std::numeric_limits<DWORD>::max() ||
      !IsSingleDerElement(der, size) || der[0] != 0x30U) {
    return CertificateStatus::kMalformed;
  }
  PCCERT_CONTEXT context = CertCreateCertificateContext(
      X509_ASN_ENCODING, der, static_cast<DWORD>(size));
  if (context == nullptr) return CertificateStatus::kMalformed;
  CertificateStatus status = CertificateStatus::kOk;
  if (key != nullptr) {
    BCRYPT_KEY_HANDLE handle = nullptr;
    std::string ignored;
    if (CryptImportPublicKeyInfoEx2(
            X509_ASN_ENCODING, &context->pCertInfo->SubjectPublicKeyInfo,
            CRYPT_OID_INFO_PUBKEY_SIGN_KEY_FLAG, nullptr, &handle) == 0 ||
        !key->Adopt(handle, &ignored)) {
      status = CertificateStatus::kKeyUnavailable;
    }
  }
  CertFreeCertificateContext(context);
  return status;
}

bool EcdsaDerToP1363(const unsigned char* der, std::size_t size,
                     std::size_t field_bytes,
                     std::vector<unsigned char>* p1363) {
  p1363->clear();
  unsigned char tag = 0;
  std::size_t content = 0;
  std::size_t length = 0;
  if (field_bytes == 0 || !ReadDerHeader(der, size, 0, &tag, &content, &length) ||
      tag != 0x30U || content + length != size) {
    return false;
  }
  std::vector<unsigned char> output(2U * field_bytes, 0);
  std::size_t cursor = content;
  for (std::size_t half = 0; half < 2; ++half) {
    std::size_t value_offset = 0;
    std::size_t value_length = 0;
    if (!ReadDerHeader(der, size, cursor, &tag, &value_offset, &value_length) ||
        tag != 0x02U || value_length == 0) {
      return false;
    }
    cursor = value_offset + value_length;
    const unsigned char* value = der + value_offset;
    if ((value[0] & 0x80U) != 0) return false;
    if (value_length > 1 && value[0] == 0) {
      if ((value[1] & 0x80U) == 0) return false;
      ++value;
      --value_length;
    }
    if (value_length > field_bytes) return false;
    bool nonzero = false;
    for (std::size_t index = 0; index < value_length; ++index) {
      nonzero = nonzero || value[index] != 0;
    }
    if (!nonzero) return false;
    std::memcpy(output.data() + half * field_bytes + (field_bytes - value_length),
                value, value_length);
  }
  if (cursor != size) return false;
  *p1363 = std::move(output);
  return true;
}

bool VerifyRsaPkcs1V15(const PublicKey& key, HashAlgorithm algorithm,
                       const std::vector<unsigned char>& digest,
                       const unsigned char* signature, std::size_t size) {
  if (!key.valid() || key.family() != KeyFamily::kRsa ||
      digest.size() != HashSize(algorithm) || signature == nullptr ||
      size != (static_cast<std::size_t>(key.bits()) + 7U) / 8U) {
    return false;
  }
  BCRYPT_PKCS1_PADDING_INFO padding = {AlgorithmName(algorithm)};
  return BCryptVerifySignature(static_cast<BCRYPT_KEY_HANDLE>(key.handle()),
                               &padding, const_cast<PUCHAR>(digest.data()),
                               static_cast<ULONG>(digest.size()),
                               const_cast<PUCHAR>(signature),
                               static_cast<ULONG>(size),
                               BCRYPT_PAD_PKCS1) == kStatusSuccess;
}

bool VerifyRsaPss(const PublicKey& key, HashAlgorithm algorithm,
                  const std::vector<unsigned char>& digest,
                  const unsigned char* signature, std::size_t size) {
  if (!key.valid() || key.family() != KeyFamily::kRsa ||
      digest.size() != HashSize(algorithm) || signature == nullptr ||
      size != (static_cast<std::size_t>(key.bits()) + 7U) / 8U) {
    return false;
  }
  BCRYPT_PSS_PADDING_INFO padding = {AlgorithmName(algorithm),
                                     static_cast<ULONG>(digest.size())};
  return BCryptVerifySignature(static_cast<BCRYPT_KEY_HANDLE>(key.handle()),
                               &padding, const_cast<PUCHAR>(digest.data()),
                               static_cast<ULONG>(digest.size()),
                               const_cast<PUCHAR>(signature),
                               static_cast<ULONG>(size),
                               BCRYPT_PAD_PSS) == kStatusSuccess;
}

bool VerifyEcdsa(const PublicKey& key, const std::vector<unsigned char>& digest,
                 const unsigned char* der_signature, std::size_t size) {
  if (!key.valid() || key.family() != KeyFamily::kEcdsa || digest.empty()) {
    return false;
  }
  const std::size_t order_bits = key.bits();
  const std::size_t field_bytes = (order_bits + 7U) / 8U;
  std::vector<unsigned char> signature;
  if (!EcdsaDerToP1363(der_signature, size, field_bytes, &signature)) {
    return false;
  }
  std::size_t hash_size = digest.size();
  if (hash_size * 8U > order_bits) {
    if (order_bits % 8U != 0) return false;
    hash_size = order_bits / 8U;
  }
  return BCryptVerifySignature(static_cast<BCRYPT_KEY_HANDLE>(key.handle()),
                               nullptr, const_cast<PUCHAR>(digest.data()),
                               static_cast<ULONG>(hash_size), signature.data(),
                               static_cast<ULONG>(signature.size()),
                               0) == kStatusSuccess;
}

bool ConstantTimeEqual(const void* left, const void* right, std::size_t size) {
  const volatile unsigned char* first =
      static_cast<const volatile unsigned char*>(left);
  const volatile unsigned char* second =
      static_cast<const volatile unsigned char*>(right);
  unsigned char difference = 0;
  for (std::size_t index = 0; index < size; ++index) {
    difference = static_cast<unsigned char>(difference | (first[index] ^ second[index]));
  }
  return difference == 0;
}

}
