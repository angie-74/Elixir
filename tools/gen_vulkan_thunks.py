#!/usr/bin/env python3
# Copyright 2026 Elixir Project Authors
# Licensed under the Apache License, Version 2.0
"""Generates the System V -> Microsoft x64 Vulkan thunks from vk.xml.

  python tools/gen_vulkan_thunks.py
  python tools/gen_vulkan_thunks.py --check   # exit 1 if outputs are stale
"""
import argparse
import collections
import os
import re
import sys
import xml.etree.ElementTree as ET

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..'))
DEFAULT_REGISTRY = os.path.join(ROOT, 'third_party', 'Vulkan-Headers', 'registry', 'vk.xml')
DEFAULT_EXPORTS = os.path.join(ROOT, 'tools', 'vulkan_ndk_libvulkan_exports.txt')
DEFAULT_OUT = os.path.join(ROOT, 'src', 'graphics', 'vulkan', 'generated')

API = 'vulkan'
MAX_CORE_VERSION = (1, 4)

FORWARDED_PLATFORMS = {None, 'win32', 'provisional'}
HOST_ONLY_PLATFORMS = {'win32'}
HANDWRITTEN_PLATFORMS = {'android'}

INTERCEPTED = [
    'vkGetInstanceProcAddr',
    'vkGetDeviceProcAddr',
    'vkEnumerateInstanceVersion',
    'vkEnumerateInstanceExtensionProperties',
    'vkEnumerateInstanceLayerProperties',
    'vkCreateInstance',
    'vkDestroyInstance',
    'vkEnumeratePhysicalDevices',
    'vkEnumeratePhysicalDeviceGroups',
    'vkEnumeratePhysicalDeviceGroupsKHR',
    'vkGetPhysicalDeviceProperties',
    'vkGetPhysicalDeviceProperties2',
    'vkGetPhysicalDeviceProperties2KHR',
    'vkEnumerateDeviceExtensionProperties',
    'vkCreateDevice',
    'vkDestroyDevice',
    'vkCreateDebugUtilsMessengerEXT',
    'vkDestroyDebugUtilsMessengerEXT',
    'vkCreateDebugReportCallbackEXT',
    'vkDestroyDebugReportCallbackEXT',
    'vkCreateAndroidSurfaceKHR',
    'vkDestroySurfaceKHR',
    'vkGetPhysicalDeviceSurfaceCapabilitiesKHR',
    'vkGetPhysicalDeviceSurfaceCapabilities2KHR',
    'vkGetPhysicalDeviceSurfacePresentModesKHR',
    'vkCreateSwapchainKHR',
    'vkAcquireNextImageKHR',
    'vkAcquireNextImage2KHR',
    'vkQueuePresentKHR',
    'vkGetSwapchainImagesKHR',
    'vkDestroySwapchainKHR',
    'vkGetDeviceQueue',
    'vkGetDeviceQueue2',
    'vkGetAndroidHardwareBufferPropertiesANDROID',
    'vkGetMemoryAndroidHardwareBufferANDROID',
    'vkGetPhysicalDeviceFeatures',
    'vkGetPhysicalDeviceFeatures2',
    'vkGetPhysicalDeviceFeatures2KHR',
    'vkGetPhysicalDeviceFormatProperties',
    'vkGetPhysicalDeviceFormatProperties2',
    'vkGetPhysicalDeviceFormatProperties2KHR',
    'vkGetPhysicalDeviceImageFormatProperties',
    'vkGetPhysicalDeviceImageFormatProperties2',
    'vkGetPhysicalDeviceImageFormatProperties2KHR',
    'vkCreateImage',
    'vkDestroyImage',
    'vkCreateImageView',
    'vkBindBufferMemory',
    'vkBindBufferMemory2',
    'vkBindBufferMemory2KHR',
    'vkMapMemory',
    'vkMapMemory2',
    'vkMapMemory2KHR',
    'vkUnmapMemory',
    'vkUnmapMemory2',
    'vkUnmapMemory2KHR',
    'vkFreeMemory',
    'vkDestroyBuffer',
    'vkCmdCopyBufferToImage',
    'vkCmdCopyBufferToImage2',
    'vkCmdCopyBufferToImage2KHR',
    'vkCmdCopyImage',
    'vkCmdCopyImage2',
    'vkCmdCopyImage2KHR',
    'vkCmdExecuteCommands',
    'vkAllocateCommandBuffers',
    'vkFreeCommandBuffers',
    'vkResetCommandPool',
    'vkDestroyCommandPool',
    'vkBeginCommandBuffer',
    'vkResetCommandBuffer',
    'vkQueueSubmit',
    'vkQueueSubmit2',
    'vkQueueSubmit2KHR',
    'vkGetDeviceImageMemoryRequirements',
    'vkGetDeviceImageMemoryRequirementsKHR',
    'vkGetDeviceImageSparseMemoryRequirements',
    'vkGetDeviceImageSparseMemoryRequirementsKHR',
    'vkGetDeviceImageSubresourceLayout',
    'vkGetDeviceImageSubresourceLayoutKHR',
    'vkCmdCopyImageToBuffer',
    'vkCmdCopyImageToBuffer2',
    'vkCmdCopyImageToBuffer2KHR',
]

SAFE_SCALARS = {
    'void', 'char', 'float', 'double', 'int', 'size_t',
    'int8_t', 'uint8_t', 'int16_t', 'uint16_t', 'int32_t', 'uint32_t',
    'int64_t', 'uint64_t',
}

LEVEL_GLOBAL, LEVEL_INSTANCE, LEVEL_DEVICE = 'kGlobal', 'kInstance', 'kDevice'


def api_ok(element):
    value = element.get('api')
    return value is None or API in value.split(',')


def decl_text(element, skip_name=False):
    parts = [element.text or '']
    for child in element:
        if child.tag == 'comment':
            parts.append(child.tail or '')
            continue
        if child.tag == 'name' and skip_name:
            parts.append(child.tail or '')
            continue
        parts.append((child.text or '') + (child.tail or ''))
    return re.sub(r'\s+', ' ', ''.join(parts)).strip()


class Param:
    def __init__(self, element):
        self.type = element.findtext('type')
        self.name = element.findtext('name')
        self.decl = decl_text(element)
        self.is_allocator = self.type == 'VkAllocationCallbacks'
        self.pointer = '*' in self.decl or '[' in self.decl


class Command:
    def __init__(self, name):
        self.name = name
        self.canonical = name
        self.ret = None
        self.params = []
        self.versions = set()
        self.extensions = []
        self.platforms = set()
        self.export = False

    @property
    def core_version(self):
        return min(self.versions) if self.versions else None


class Registry:
    def __init__(self, path):
        self.root = ET.parse(path).getroot()
        self.types = {}
        self.handles = {}
        self.dispatchable = set()
        self.defines = {}
        self.commands = {}
        self.aliases = {}
        self.platform_of = {}
        self.extension_info = {}
        self._parse_types()
        self._parse_commands()

    def _parse_types(self):
        for t in self.root.find('types'):
            if t.tag != 'type' or not api_ok(t):
                continue
            category = t.get('category')
            name = t.get('name') or t.findtext('name')
            if category == 'funcpointer' and name is None:
                proto = t.find('proto')
                name = proto.findtext('name') if proto is not None else None
            if name is None:
                continue
            self.types[name] = t
            if category == 'handle':
                if t.get('alias'):
                    self.handles[name] = t.get('alias')
                else:
                    self.handles[name] = name
                    if t.findtext('type') == 'VK_DEFINE_HANDLE':
                        self.dispatchable.add(name)
            if category == 'define':
                text = ''.join(t.itertext())
                self.defines[name] = text
        match = re.search(r'VK_HEADER_VERSION\s+(\d+)', self.defines.get('VK_HEADER_VERSION', ''))
        if not match:
            raise SystemExit('vk.xml: VK_HEADER_VERSION not found')
        self.header_version = int(match.group(1))
        complete = self.defines.get('VK_HEADER_VERSION_COMPLETE', '')
        match = re.search(r'VK_MAKE_API_VERSION\s*\(\s*0\s*,\s*(\d+)\s*,\s*(\d+)\s*,\s*VK_HEADER_VERSION', complete)
        if not match:
            raise SystemExit('vk.xml: VK_HEADER_VERSION_COMPLETE not understood')
        self.header_api = (int(match.group(1)), int(match.group(2)))
        self.api_version_defines = {}
        for name, text in self.defines.items():
            m = re.fullmatch(r'VK_API_VERSION_(\d+)_(\d+)', name)
            if m and re.search(r'VK_MAKE_API_VERSION\s*\(\s*0\s*,', text):
                self.api_version_defines[(int(m.group(1)), int(m.group(2)))] = name

    def _parse_commands(self):
        for c in self.root.find('commands'):
            if c.tag != 'command' or not api_ok(c):
                continue
            if c.get('alias'):
                self.aliases[c.get('name')] = c.get('alias')
                continue
            proto = c.find('proto')
            name = proto.findtext('name')
            cmd = Command(name)
            cmd.ret = decl_text(proto, skip_name=True)
            cmd.params = [Param(p) for p in c.findall('param') if api_ok(p)]
            cmd.export = API in (c.get('export') or '').split(',')
            self.commands[name] = cmd

    def canonical(self, name):
        seen = set()
        while name in self.aliases:
            if name in seen:
                raise SystemExit('alias loop at ' + name)
            seen.add(name)
            name = self.aliases[name]
        return name


def collect(reg):
    selected = {}
    excluded = collections.Counter()
    excluded_commands = collections.defaultdict(list)

    def entry(name):
        if name not in selected:
            canonical = reg.canonical(name)
            if canonical not in reg.commands:
                raise SystemExit('command %s (-> %s) has no definition' % (name, canonical))
            base = reg.commands[canonical]
            cmd = Command(name)
            cmd.canonical = canonical
            cmd.ret = base.ret
            cmd.params = base.params
            cmd.export = base.export
            selected[name] = cmd
        return selected[name]

    for feature in reg.root.findall('feature'):
        if API not in (feature.get('api') or '').split(','):
            continue
        major, minor = (int(x) for x in feature.get('number').split('.'))
        if (major, minor) > MAX_CORE_VERSION:
            continue
        for require in feature.findall('require'):
            if not api_ok(require):
                continue
            for c in require.findall('command'):
                entry(c.get('name')).versions.add((major, minor))

    extension_names = []
    for ext in reg.root.find('extensions'):
        name = ext.get('name')
        supported = (ext.get('supported') or '').split(',')
        platform = ext.get('platform')
        reg.extension_info[name] = (platform, ext.get('type'), API in supported)
        if API not in supported:
            continue
        commands = [c.get('name') for r in ext.findall('require') if api_ok(r) for c in r.findall('command')]
        if platform not in FORWARDED_PLATFORMS and platform not in HANDWRITTEN_PLATFORMS:
            for c in commands:
                if c not in selected:
                    excluded[platform] += 1
                    excluded_commands[platform].append(c)
            continue
        extension_names.append(name)
        for c in commands:
            cmd = entry(c)
            if name not in cmd.extensions:
                cmd.extensions.append(name)
            cmd.platforms.add(platform)
    for platform in list(excluded_commands):
        remaining = [c for c in excluded_commands[platform] if c not in selected]
        excluded_commands[platform] = sorted(set(remaining))
        excluded[platform] = len(excluded_commands[platform])
    return selected, excluded, excluded_commands


def classify(reg, cmd):
    if cmd.canonical in ('vkGetInstanceProcAddr', 'vkCreateInstance',
                         'vkEnumerateInstanceVersion',
                         'vkEnumerateInstanceExtensionProperties',
                         'vkEnumerateInstanceLayerProperties'):
        return LEVEL_GLOBAL
    first = reg.handles.get(cmd.params[0].type) if cmd.params else None
    if first in ('VkInstance', 'VkPhysicalDevice'):
        return LEVEL_INSTANCE
    if first in ('VkDevice', 'VkQueue', 'VkCommandBuffer', 'VkExternalComputeQueueNV'):
        return LEVEL_DEVICE
    raise SystemExit('cannot classify %s (first parameter %s)' % (cmd.name, cmd.params[0].type if cmd.params else None))


def type_ok(reg, param):
    type_name = param.type
    if type_name in SAFE_SCALARS:
        return True
    if type_name.startswith('StdVideo'):
        return True
    t = reg.types.get(type_name)
    if t is None:
        return False
    category = t.get('category')
    if category in ('struct', 'union', 'handle', 'enum', 'bitmask', 'funcpointer'):
        return True
    if category == 'basetype':
        text = ''.join(t.itertext())
        if 'long' in text.split() or 'wchar_t' in text:
            return False
        return param.pointer or 'typedef' in text
    return category is None and param.pointer


def struct_stypes(reg, included_extensions):
    required_types = set()
    for feature in reg.root.findall('feature'):
        if API not in (feature.get('api') or '').split(','):
            continue
        major, minor = (int(x) for x in feature.get('number').split('.'))
        if (major, minor) > MAX_CORE_VERSION:
            continue
        for require in feature.findall('require'):
            if api_ok(require):
                required_types.update(t.get('name') for t in require.findall('type'))
    for ext in reg.root.find('extensions'):
        if ext.get('name') not in included_extensions:
            continue
        for require in ext.findall('require'):
            if api_ok(require):
                required_types.update(t.get('name') for t in require.findall('type'))
    result = []
    seen = set()
    for name, t in reg.types.items():
        if t.get('category') not in ('struct', 'union') or t.get('alias'):
            continue
        if name not in required_types:
            continue
        for member in t.findall('member'):
            if not api_ok(member):
                continue
            if member.findtext('name') == 'sType' and member.get('values'):
                value = member.get('values')
                if value not in seen:
                    seen.add(value)
                    result.append((value, name))
            break
    result.sort(key=lambda item: item[1])
    return result


HEADER_TEMPLATE = '''\
// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
//
// GENERATED - do not edit. Produced by tools/gen_vulkan_thunks.py from
// third_party/Vulkan-Headers/registry/vk.xml (VK_HEADER_VERSION {header_version}).
// Regenerate with:  python tools/gen_vulkan_thunks.py
#ifndef ELIXIR_GRAPHICS_VULKAN_GENERATED_VK_SYSV_THUNKS_H_
#define ELIXIR_GRAPHICS_VULKAN_GENERATED_VK_SYSV_THUNKS_H_

#include <cstddef>
#include <cstdint>
#include <string_view>

#include "abi/sysv_abi.h"
#include "graphics/vulkan/vulkan_host_headers.h"

namespace elixir::vulkan::generated {{

inline constexpr std::uint32_t kRegistryHeaderVersion = {header_version};
inline constexpr std::uint32_t kMaxApiVersion = VK_MAKE_API_VERSION(0, {max_major}, {max_minor}, {header_version});

enum class CommandLevel : std::uint8_t {{
  kGlobal,
  kInstance,
  kDevice,
}};

enum CommandFlag : std::uint8_t {{
  kCommandCore = 1 << 0,
  kCommandIntercepted = 1 << 1,
  kCommandHostOnly = 1 << 2,
  kCommandAndroid = 1 << 3,
  kCommandAlias = 1 << 4,
  kCommandProvisional = 1 << 5,
  kCommandLibvulkanExport = 1 << 6,
}};

struct CommandInfo {{
  const char* name;
  void* guest_proc;
  std::uint32_t core_version;
  const char* extensions;
  CommandLevel level;
  std::uint8_t flags;
}};

inline constexpr std::size_t kCommandCount = {total};
extern const CommandInfo kCommands[kCommandCount];

extern PFN_vkVoidFunction g_host_dispatch[kCommandCount];

[[gnu::always_inline]] inline PFN_vkVoidFunction HostSlot(std::size_t index) noexcept {{
  return __atomic_load_n(&g_host_dispatch[index], __ATOMIC_RELAXED);
}}
[[gnu::always_inline]] inline void SetHostSlot(std::size_t index, PFN_vkVoidFunction entry) noexcept {{
  __atomic_store_n(&g_host_dispatch[index], entry, __ATOMIC_RELAXED);
}}
[[gnu::always_inline]] inline PFN_vkVoidFunction ExchangeHostSlot(std::size_t index,
                                                                  PFN_vkVoidFunction entry) noexcept {{
  return __atomic_exchange_n(&g_host_dispatch[index], entry, __ATOMIC_RELAXED);
}}

const CommandInfo* FindCommand(const char* name) noexcept;
inline std::size_t CommandIndex(const CommandInfo* info) noexcept {{
  return static_cast<std::size_t>(info - kCommands);
}}

std::size_t StructSizeForSType(VkStructureType type) noexcept;

inline constexpr std::size_t kLibvulkanExportCount = {export_count};
extern const char* const kLibvulkanExports[kLibvulkanExportCount];

inline constexpr std::size_t kGuestExtensionCount = {guest_ext_count};
extern const char* const kGuestExtensions[kGuestExtensionCount];
bool IsGuestExtension(std::string_view name) noexcept;

inline constexpr std::size_t kHostOnlyExtensionCount = {host_only_ext_count};
extern const char* const kHostOnlyExtensions[kHostOnlyExtensionCount];
inline constexpr std::size_t kAndroidExtensionCount = {android_ext_count};
extern const char* const kAndroidExtensions[kAndroidExtensionCount];

enum CommandIndexValue : std::uint16_t {{
{enum_values}
}};

}}

#define ELIXIR_VK_HOST(cmd)                                                   \\
  (reinterpret_cast<PFN_##cmd>(__atomic_load_n(                              \\
      &::elixir::vulkan::generated::g_host_dispatch                           \\
           [::elixir::vulkan::generated::kCmd_##cmd],                         \\
      __ATOMIC_RELAXED)))

extern "C" {{
{intercept_decls}
}}

#endif
'''

SOURCE_TEMPLATE = '''\
// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
//
// GENERATED - do not edit. Produced by tools/gen_vulkan_thunks.py from
// third_party/Vulkan-Headers/registry/vk.xml (VK_HEADER_VERSION {header_version}).
// Regenerate with:  python tools/gen_vulkan_thunks.py
#include "graphics/vulkan/generated/vk_sysv_thunks.h"

#include <cstring>
#include <type_traits>

namespace elixir::vulkan::generated {{

PFN_vkVoidFunction g_host_dispatch[kCommandCount] = {{}};

}}

extern "C" {{
{thunks}
}}

namespace elixir::vulkan::generated {{
namespace {{

template <typename HostFn>
struct GuestSignature;
template <typename R, typename... A>
struct GuestSignature<R(VKAPI_PTR*)(A...)> {{
  using type = R(ELIXIR_SYSV*)(A...);
}};

}}

#define ELIXIR_VK_SIGNATURE(cmd, fn) \\
  static_assert(std::is_same_v<decltype(&fn), GuestSignature<PFN_##cmd>::type>, #fn " differs from PFN_" #cmd);
{signature_checks}
#undef ELIXIR_VK_SIGNATURE

#define ELIXIR_VK_PROC(fn) reinterpret_cast<void*>(&fn)

const CommandInfo kCommands[kCommandCount] = {{
{table}
}};

#undef ELIXIR_VK_PROC

const CommandInfo* FindCommand(const char* name) noexcept {{
  if (name == nullptr) return nullptr;
  std::size_t low = 0;
  std::size_t high = kCommandCount;
  while (low < high) {{
    const std::size_t mid = low + (high - low) / 2;
    const int order = std::strcmp(kCommands[mid].name, name);
    if (order == 0) return &kCommands[mid];
    if (order < 0) {{
      low = mid + 1;
    }} else {{
      high = mid;
    }}
  }}
  return nullptr;
}}

std::size_t StructSizeForSType(VkStructureType type) noexcept {{
  switch (type) {{
{stype_cases}
    default:
      return 0;
  }}
}}

const char* const kLibvulkanExports[kLibvulkanExportCount] = {{
{exports}
}};

const char* const kGuestExtensions[kGuestExtensionCount] = {{
{guest_exts}
}};

bool IsGuestExtension(std::string_view name) noexcept {{
  std::size_t low = 0;
  std::size_t high = kGuestExtensionCount;
  while (low < high) {{
    const std::size_t mid = low + (high - low) / 2;
    const int order = std::string_view(kGuestExtensions[mid]).compare(name);
    if (order == 0) return true;
    if (order < 0) {{
      low = mid + 1;
    }} else {{
      high = mid;
    }}
  }}
  return false;
}}

const char* const kHostOnlyExtensions[kHostOnlyExtensionCount] = {{
{host_only_exts}
}};

const char* const kAndroidExtensions[kAndroidExtensionCount] = {{
{android_exts}
}};

}}
'''


def c_string(text):
    return '"' + text.replace('\\', '\\\\').replace('"', '\\"') + '"'


def generate(registry_path, exports_path):
    reg = Registry(registry_path)
    selected, excluded, excluded_commands = collect(reg)

    for name in INTERCEPTED:
        if name not in selected:
            raise SystemExit('INTERCEPTED command %s is not in the selected set' % name)
    intercepted = set(INTERCEPTED)

    with open(exports_path, encoding='ascii') as f:
        exports = [line.strip() for line in f if line.strip() and not line.startswith('#')]
    exports = sorted(set(exports))
    for name in exports:
        if name not in selected:
            raise SystemExit('libvulkan.so export %s is not a selected command' % name)

    infos = []
    for name in sorted(selected, key=lambda n: n.encode('ascii')):
        cmd = selected[name]
        level = classify(reg, cmd)
        android = bool(cmd.platforms & HANDWRITTEN_PLATFORMS) and not cmd.versions
        host_only = (not cmd.versions and cmd.platforms and cmd.platforms <= HOST_ONLY_PLATFORMS)
        provisional = (not cmd.versions and 'provisional' in cmd.platforms)
        if android and name not in intercepted:
            raise SystemExit('Android platform command %s needs a hand-written interceptor' % name)
        if host_only and name in intercepted:
            raise SystemExit('host-only command %s cannot be intercepted' % name)
        if not host_only:
            for p in cmd.params:
                if not type_ok(reg, p):
                    raise SystemExit('%s: parameter %s has ABI-unsafe type %s' % (name, p.name, p.type))
        flags = []
        if cmd.versions:
            flags.append('kCommandCore')
        if name in intercepted:
            flags.append('kCommandIntercepted')
        if host_only:
            flags.append('kCommandHostOnly')
        if android:
            flags.append('kCommandAndroid')
        if name != cmd.canonical:
            flags.append('kCommandAlias')
        if provisional:
            flags.append('kCommandProvisional')
        if name in exports:
            flags.append('kCommandLibvulkanExport')
        infos.append((name, cmd, level, flags, host_only, android))

    thunks = []
    table = []
    intercept_decls = []
    enum_values = []
    signature_checks = []
    for index, (name, cmd, level, flags, host_only, android) in enumerate(infos):
        enum_values.append('  kCmd_%s = %d,' % (name, index))
        params_decl = ', '.join(p.decl for p in cmd.params) or 'void'
        if name in intercepted:
            proc = 'ELIXIR_VK_PROC(elixir_vk_intercept_%s)' % name
            intercept_decls.append('ELIXIR_SYSV %s elixir_vk_intercept_%s(%s);' % (cmd.ret, name, params_decl))
            signature_checks.append('ELIXIR_VK_SIGNATURE(%s, elixir_vk_intercept_%s)' % (name, name))
        elif host_only:
            proc = 'nullptr'
        else:
            proc = 'ELIXIR_VK_PROC(elixir_vk_sysv_%s)' % name
            signature_checks.append('ELIXIR_VK_SIGNATURE(%s, elixir_vk_sysv_%s)' % (name, name))
            wrapper_params = []
            args = []
            for p in cmd.params:
                if p.is_allocator:
                    wrapper_params.append(p.decl.rsplit(p.name, 1)[0].rstrip())
                    args.append('nullptr')
                else:
                    wrapper_params.append(p.decl)
                    args.append(p.name)
            ret_kw = '' if cmd.ret == 'void' else 'return '
            thunks.append('ELIXIR_SYSV %s elixir_vk_sysv_%s(%s) {\n  %sELIXIR_VK_HOST(%s)(%s);\n}'
                          % (cmd.ret, name, ', '.join(wrapper_params) or 'void', ret_kw, name, ', '.join(args)))
        version = cmd.core_version
        version_text = reg.api_version_defines[version] if version else '0'
        ext_text = c_string(','.join(cmd.extensions)) if cmd.extensions else 'nullptr'
        flag_text = ' | '.join(flags) if flags else '0'
        table.append('    {%s, %s, %s, %s, CommandLevel::%s, %s},'
                     % (c_string(name), proc, version_text, ext_text, level, flag_text))

    included_extensions = set()
    host_only_exts = []
    android_exts = []
    guest_exts = []
    for ext_name, (platform, _, supported) in reg.extension_info.items():
        if platform in HOST_ONLY_PLATFORMS and supported:
            host_only_exts.append(ext_name)
        if platform in HANDWRITTEN_PLATFORMS:
            android_exts.append(ext_name)
        if supported and (platform in FORWARDED_PLATFORMS or platform in HANDWRITTEN_PLATFORMS):
            included_extensions.add(ext_name)
        if supported and platform in FORWARDED_PLATFORMS and platform not in HOST_ONLY_PLATFORMS:
            guest_exts.append(ext_name)
    host_only_exts.sort()
    android_exts.sort()
    guest_exts.sort(key=lambda n: n.encode('ascii'))
    by_name = {i[0]: i for i in infos}
    guest_ext_set = set(guest_exts)
    for name, cmd, level, flags, host_only, android in infos:
        if host_only and any(e in guest_ext_set for e in cmd.extensions):
            raise SystemExit('%s belongs to a guest-visible extension but is host-only' % name)
    for ext in reg.root.find('extensions'):
        if ext.get('name') not in guest_ext_set:
            continue
        for r in ext.findall('require'):
            if not api_ok(r):
                continue
            for c in r.findall('command'):
                if c.get('name') not in by_name:
                    raise SystemExit('guest-visible extension %s: command %s is not forwarded'
                                     % (ext.get('name'), c.get('name')))

    stypes = struct_stypes(reg, included_extensions)
    stype_cases = '\n'.join('    case %s:\n      return sizeof(%s);' % (value, struct) for value, struct in stypes)

    core = [i for i in infos if i[1].versions]
    by_version = collections.Counter(i[1].core_version for i in core)

    stats = dict(
        header_version=reg.header_version,
        total=len(infos),
        core=len(core),
        ext_only=len(infos) - len(core),
        aliases=sum(1 for i in infos if 'kCommandAlias' in i[3]),
        guest_visible=sum(1 for i in infos if not i[4]),
        intercepted=len(intercepted),
        host_only=sum(1 for i in infos if i[4]),
        max_major=MAX_CORE_VERSION[0],
        max_minor=MAX_CORE_VERSION[1],
        export_count=len(exports),
        host_only_ext_count=len(host_only_exts),
        android_ext_count=len(android_exts),
        guest_ext_count=len(guest_exts),
    )
    header = HEADER_TEMPLATE.format(enum_values='\n'.join(enum_values),
                                    intercept_decls='\n'.join(intercept_decls), **stats)
    source = SOURCE_TEMPLATE.format(
        header_version=reg.header_version,
        thunks='\n\n'.join(thunks),
        signature_checks='\n'.join(signature_checks),
        table='\n'.join(table),
        stype_cases=stype_cases,
        exports='\n'.join('    %s,' % c_string(n) for n in exports),
        guest_exts='\n'.join('    %s,' % c_string(n) for n in guest_exts),
        host_only_exts='\n'.join('    %s,' % c_string(n) for n in host_only_exts),
        android_exts='\n'.join('    %s,' % c_string(n) for n in android_exts))
    stats['wrappers'] = len(thunks)
    stats['signatures'] = len(signature_checks)
    stats['stypes'] = len(stypes)
    stats['excluded'] = dict(excluded)
    stats['by_version'] = {'%d.%d' % v: n for v, n in sorted(by_version.items())}
    return header, source, stats


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--registry', default=DEFAULT_REGISTRY)
    parser.add_argument('--exports', default=DEFAULT_EXPORTS)
    parser.add_argument('--out', default=DEFAULT_OUT)
    parser.add_argument('--check', action='store_true', help='fail if the committed outputs differ')
    args = parser.parse_args()

    header, source, stats = generate(args.registry, args.exports)
    outputs = {
        os.path.join(args.out, 'vk_sysv_thunks.h'): header,
        os.path.join(args.out, 'vk_sysv_thunks.cc'): source,
    }
    stale = []
    for path, text in outputs.items():
        current = None
        if os.path.exists(path):
            with open(path, encoding='utf-8', newline='') as f:
                current = f.read().replace('\r\n', '\n')
        if current != text:
            stale.append(path)
            if not args.check:
                os.makedirs(os.path.dirname(path), exist_ok=True)
                with open(path, 'w', encoding='utf-8', newline='\n') as f:
                    f.write(text)
    summary = ('vk.xml %(header_version)d: %(total)d commands (%(core)d core, %(ext_only)d extension-only, '
               '%(aliases)d aliases), %(guest_visible)d guest-visible, %(wrappers)d generated wrappers, '
               '%(intercepted)d interceptors, %(host_only)d host-only, %(signatures)d signature checks, '
               '%(stypes)d sTypes, %(guest_ext_count)d guest-visible extensions, '
               '%(export_count)d libvulkan.so exports' % stats)
    print(summary)
    print('core by version: %s' % stats['by_version'])
    print('not forwarded by platform: %s' % stats['excluded'])
    if args.check and stale:
        print('stale generated files: ' + ', '.join(stale), file=sys.stderr)
        return 1
    return 0


if __name__ == '__main__':
    sys.exit(main())
