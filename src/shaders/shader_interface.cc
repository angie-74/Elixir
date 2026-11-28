// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#include "shaders/shader_interface.h"

#include <algorithm>
#include <map>
#include <set>

namespace elixir::shaders {

namespace {

constexpr std::uint32_t kMagic = 0x07230203;

enum Op : std::uint32_t {
  kOpName = 5,
  kOpMemberName = 6,
  kOpEntryPoint = 15,
  kOpCapability = 17,
  kOpTypeInt = 21,
  kOpTypeFloat = 22,
  kOpTypeVector = 23,
  kOpTypeMatrix = 24,
  kOpTypeImage = 25,
  kOpTypeSampledImage = 27,
  kOpTypeArray = 28,
  kOpTypeStruct = 30,
  kOpTypePointer = 32,
  kOpConstant = 43,
  kOpVariable = 59,
  kOpDecorate = 71,
  kOpMemberDecorate = 72,
};

enum Decoration : std::uint32_t {
  kBlock = 2,
  kBufferBlock = 3,
  kArrayStride = 6,
  kMatrixStride = 7,
  kBuiltIn = 11,
  kLocation = 30,
  kBinding = 33,
  kDescriptorSet = 34,
  kOffset = 35,
};

enum StorageClass : std::uint32_t {
  kUniformConstant = 0,
  kInput = 1,
  kUniform = 2,
  kOutput = 3,
  kPrivate = 6,
  kFunction = 7,
};

struct Type {
  std::uint32_t op = 0;
  std::uint32_t a = 0;
  std::uint32_t b = 0;
  std::uint32_t c = 0;
  std::uint32_t d = 0;
  std::uint32_t e = 0;
  std::vector<std::uint32_t> members;
};

struct Variable {
  std::uint32_t id = 0;
  std::uint32_t pointer_type = 0;
  std::uint32_t storage = 0;
};

std::string Text(const std::uint32_t* words, std::size_t count) {
  std::string out;
  for (std::size_t i = 0; i < count; ++i) {
    for (int shift = 0; shift < 32; shift += 8) {
      const char c = static_cast<char>((words[i] >> shift) & 0xFF);
      if (c == '\0') return out;
      out.push_back(c);
    }
  }
  return out;
}

bool Fail(std::string* problem, std::string text) {
  if (problem != nullptr) *problem = std::move(text);
  return false;
}

}

bool ReadShaderInterface(const std::vector<std::uint32_t>& spirv, ShaderStage stage, ShaderInterface* out,
                         std::string* problem) {
  *out = ShaderInterface{};
  if (spirv.size() < 5 || spirv[0] != kMagic) return Fail(problem, "the compiled shader is not SPIR-V");
  std::map<std::uint32_t, std::string> names;
  std::map<std::uint64_t, std::string> member_names;
  std::map<std::uint32_t, Type> types;
  std::map<std::uint32_t, std::uint32_t> constants;
  std::map<std::uint32_t, std::map<std::uint32_t, std::uint32_t>> decorations;
  std::map<std::uint64_t, std::map<std::uint32_t, std::uint32_t>> member_decorations;
  std::vector<Variable> variables;
  std::set<std::uint32_t> capabilities;
  int entry_points = 0;
  std::uint32_t model = 0xFFFFFFFF;

  for (std::size_t at = 5; at < spirv.size();) {
    const std::uint32_t count = spirv[at] >> 16;
    const std::uint32_t op = spirv[at] & 0xFFFF;
    if (count == 0 || at + count > spirv.size()) return Fail(problem, "the compiled shader is damaged");
    const std::uint32_t* w = &spirv[at];
    switch (op) {
      case kOpName:
        if (count >= 3) names[w[1]] = Text(w + 2, count - 2);
        break;
      case kOpMemberName:
        if (count >= 4) member_names[(static_cast<std::uint64_t>(w[1]) << 32) | w[2]] = Text(w + 3, count - 3);
        break;
      case kOpEntryPoint:
        ++entry_points;
        if (count >= 2) model = w[1];
        break;
      case kOpCapability:
        if (count >= 2) capabilities.insert(w[1]);
        break;
      case kOpTypeInt:
      case kOpTypeFloat:
      case kOpTypeVector:
      case kOpTypeMatrix:
      case kOpTypeImage:
      case kOpTypeSampledImage:
      case kOpTypeArray:
      case kOpTypePointer: {
        if (count < 2) break;
        Type type;
        type.op = op;
        if (count > 2) type.a = w[2];
        if (count > 3) type.b = w[3];
        if (count > 4) type.c = w[4];
        if (count > 5) type.d = w[5];
        if (count > 6) type.e = w[6];
        types[w[1]] = std::move(type);
        break;
      }
      case kOpTypeStruct: {
        if (count < 2) break;
        Type type;
        type.op = op;
        type.members.assign(w + 2, w + count);
        types[w[1]] = std::move(type);
        break;
      }
      case kOpConstant:
        if (count >= 4) constants[w[2]] = w[3];
        break;
      case kOpVariable:
        if (count >= 4) variables.push_back(Variable{w[2], w[1], w[3]});
        break;
      case kOpDecorate:
        if (count >= 3) decorations[w[1]][w[2]] = count > 3 ? w[3] : 1;
        break;
      case kOpMemberDecorate:
        if (count >= 4) {
          member_decorations[(static_cast<std::uint64_t>(w[1]) << 32) | w[2]][w[3]] = count > 4 ? w[4] : 1;
        }
        break;
      default:
        break;
    }
    at += count;
  }

  static constexpr std::uint32_t kAllowedCapabilities[] = {0, 1, 25, 50, 51};
  for (const std::uint32_t capability : capabilities) {
    if (std::find(std::begin(kAllowedCapabilities), std::end(kAllowedCapabilities), capability) ==
        std::end(kAllowedCapabilities)) {
      return Fail(problem, "the shader uses a feature Elixir does not allow (SPIR-V capability " +
                               std::to_string(capability) + ")");
    }
  }
  const std::uint32_t wanted_model = stage == ShaderStage::kVertex ? 0u : stage == ShaderStage::kFragment ? 4u : 5u;
  if (entry_points != 1 || model != wanted_model) return Fail(problem, "the shader needs exactly one main()");

  const auto decoration = [&decorations](std::uint32_t id, std::uint32_t which, std::uint32_t* value) {
    const auto found = decorations.find(id);
    if (found == decorations.end()) return false;
    const auto entry = found->second.find(which);
    if (entry == found->second.end()) return false;
    *value = entry->second;
    return true;
  };
  const auto type_of = [&types](std::uint32_t id) -> const Type* {
    const auto found = types.find(id);
    return found == types.end() ? nullptr : &found->second;
  };
  const auto scalar = [&type_of](std::uint32_t id, std::uint32_t* op, std::uint32_t* is_signed) {
    const Type* type = type_of(id);
    if (type == nullptr || (type->op != kOpTypeFloat && type->op != kOpTypeInt) || type->a != 32) return false;
    *op = type->op;
    *is_signed = type->op == kOpTypeInt ? type->b : 0;
    return true;
  };

  for (const Variable& variable : variables) {
    if (variable.storage == kPrivate || variable.storage == kFunction) continue;
    const Type* pointer = type_of(variable.pointer_type);
    if (pointer == nullptr || pointer->op != kOpTypePointer) return Fail(problem, "the compiled shader is damaged");
    const std::uint32_t pointee_id = pointer->b;
    const Type* pointee = type_of(pointee_id);
    std::uint32_t value = 0;
    const auto name = names.find(variable.id);
    const std::string variable_name = name == names.end() ? std::string() : name->second;
    if (variable.storage == kInput || variable.storage == kOutput) {
      if (decoration(variable.id, kBuiltIn, &value)) continue;
      if (pointee != nullptr && pointee->op == kOpTypeStruct) continue;
      if (stage == ShaderStage::kVertex && variable.storage == kInput) {
        return Fail(problem, "a vertex shader gets no inputs besides gl_VertexIndex");
      }
      if (!decoration(variable.id, kLocation, &value) || value != 0) {
        return Fail(problem, "only location 0 is available: the picture's coordinate in, and one colour out");
      }
      continue;
    }
    if (variable.storage == kUniformConstant) {
      const Type* image = pointee != nullptr && pointee->op == kOpTypeSampledImage ? type_of(pointee->a) : nullptr;
      if (image == nullptr || image->op != kOpTypeImage || image->b != 1 || image->d != 0 || image->e != 0) {
        return Fail(problem, "textures must be declared as sampler2D");
      }
      std::uint32_t set = 0;
      std::uint32_t binding = 0;
      if (!decoration(variable.id, kDescriptorSet, &set) || set != 0 || !decoration(variable.id, kBinding, &binding) ||
          binding >= kMaxBindings) {
        return Fail(problem, "every sampler2D needs layout(set = 0, binding = 0 to 15)");
      }
      if (variable_name.empty()) return Fail(problem, "a sampler2D has no name to match");
      out->samplers.push_back(SamplerBinding{variable_name, binding});
      continue;
    }
    if (variable.storage == kUniform) {
      if (pointee == nullptr || pointee->op != kOpTypeStruct || !decoration(pointee_id, kBlock, &value)) {
        return Fail(problem, "only uniform blocks and sampler2D are available to a shader");
      }
      std::uint32_t set = 0;
      UniformBlock block;
      if (!decoration(variable.id, kDescriptorSet, &set) || set != 0 ||
          !decoration(variable.id, kBinding, &block.binding) || block.binding >= kMaxBindings) {
        return Fail(problem, "every uniform block needs layout(set = 0, binding = 0 to 15)");
      }
      for (std::uint32_t index = 0; index < pointee->members.size(); ++index) {
        const std::uint64_t key = (static_cast<std::uint64_t>(pointee_id) << 32) | index;
        UniformMember member;
        const auto member_name = member_names.find(key);
        if (member_name != member_names.end()) member.name = member_name->second;
        const auto member_decoration = member_decorations.find(key);
        if (member_decoration == member_decorations.end() || member_decoration->second.count(kOffset) == 0) {
          return Fail(problem, "a uniform block member has no offset");
        }
        member.offset = member_decoration->second.at(kOffset);
        const std::uint32_t member_type_id = pointee->members[index];
        const Type* member_type = type_of(member_type_id);
        std::uint32_t op = 0;
        std::uint32_t is_signed = 0;
        if (scalar(member_type_id, &op, &is_signed)) {
          member.size = 4;
          member.type = op == kOpTypeFloat ? UniformType::kFloat : is_signed != 0 ? UniformType::kInt : UniformType::kUint;
        } else if (member_type != nullptr && member_type->op == kOpTypeVector &&
                   scalar(member_type->a, &op, &is_signed) && member_type->b >= 2 && member_type->b <= 4) {
          member.size = 4 * member_type->b;
          if (op == kOpTypeFloat) {
            member.type = member_type->b == 2   ? UniformType::kVec2
                          : member_type->b == 3 ? UniformType::kVec3
                                                : UniformType::kVec4;
          } else {
            member.type = member_type->b == 2   ? UniformType::kIvec2
                          : member_type->b == 4 ? UniformType::kIvec4
                                                : UniformType::kOther;
          }
        } else if (member_type != nullptr && member_type->op == kOpTypeMatrix) {
          const std::uint32_t stride = member_decoration->second.count(kMatrixStride) != 0
                                           ? member_decoration->second.at(kMatrixStride)
                                           : 16;
          member.size = stride * member_type->b;
        } else if (member_type != nullptr && member_type->op == kOpTypeArray) {
          std::uint32_t stride = 0;
          const auto length = constants.find(member_type->b);
          if (!decoration(member_type_id, kArrayStride, &stride) || length == constants.end() ||
              length->second > 256) {
            return Fail(problem, "a uniform block array is too large or has no size");
          }
          member.size = stride * length->second;
        } else {
          return Fail(problem, "uniform block members must be numbers, vectors, matrices or arrays of them");
        }
        if (member.offset > kMaxUniformBlockBytes || member.size > kMaxUniformBlockBytes) {
          return Fail(problem, "a uniform block is larger than 4096 bytes");
        }
        block.size = std::max(block.size, member.offset + member.size);
        block.members.push_back(std::move(member));
      }
      block.size = (block.size + 15u) & ~15u;
      if (block.size == 0 || block.size > kMaxUniformBlockBytes) {
        return Fail(problem, "a uniform block is empty or larger than 4096 bytes");
      }
      out->blocks.push_back(std::move(block));
      continue;
    }
    return Fail(problem, "only uniform blocks and sampler2D are available to a shader");
  }

  std::set<std::uint32_t> used;
  for (const UniformBlock& block : out->blocks) {
    if (!used.insert(block.binding).second) return Fail(problem, "two resources share a binding");
  }
  for (const SamplerBinding& sampler : out->samplers) {
    if (!used.insert(sampler.binding).second) return Fail(problem, "two resources share a binding");
  }
  return true;
}

}
