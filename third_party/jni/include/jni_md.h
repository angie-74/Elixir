/*
 * Copyright (c) 1996, 2020, Oracle and/or its affiliates. All rights reserved.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.  Oracle designates this
 * particular file as subject to the "Classpath" exception as provided
 * by Oracle in the LICENSE file that accompanied this code.
 *
 * This code is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * version 2 for more details (a copy is included in the LICENSE file that
 * accompanied this code).
 *
 * You should have received a copy of the GNU General Public License version
 * 2 along with this work; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Please contact Oracle, 500 Oracle Parkway, Redwood Shores, CA 94065 USA
 * or visit www.oracle.com if you need additional information or have any
 * questions.
 */

/*
 * Modified by the Elixir Project Authors (2026) for the native Windows x86-64
 * host: on _WIN32 the JNI function tables are called by Android x86_64 guest
 * code, so JNICALL selects the System V AMD64 calling convention and the
 * primitive typedefs keep their LP64/Android widths. The non-Windows
 * (upstream Mocktail / Linux) definitions below are unchanged.
 */

#ifndef _JAVASOFT_JNI_MD_H_
#define _JAVASOFT_JNI_MD_H_

#ifndef __has_attribute
  #define __has_attribute(x) 0
#endif

#if defined(_WIN32)
  #if !defined(__clang__)
    #error "Elixir's JNI headers need clang (clang-cl): JNICALL is __attribute__((sysv_abi)) on Windows"
  #endif
  #if !defined(_M_X64) && !defined(__x86_64__)
    #error "Elixir's JNI headers target x86-64 Windows hosting x86_64 Android guests"
  #endif
  /* Every JNINativeInterface_ / JNIInvokeInterface_ slot, every registered
     native and every Java_* / JNI_OnLoad guest export uses the guest (System
     V) convention. Also enables the System V host forwarding wrappers in
     jni.h (see ELIXIR_JNI_SYSV there). */
  #define JNICALL __attribute__((sysv_abi))
  #define ELIXIR_JNI_SYSV 1
  /* Host-implemented JNI entry points reach guest code through the ELF
     loader's symbol maps, never through the PE export table, so they need
     neither dllexport nor ELF visibility attributes. */
  #ifndef JNIEXPORT
    #define JNIEXPORT
  #endif
  #define JNIIMPORT

  typedef int jint;
  /* LP64 Android: jlong is 64-bit. Windows is LLP64 (long is 32-bit). */
  typedef long long jlong;
  typedef signed char jbyte;
  typedef char elixir_jni_md_jlong_is_64_bit[sizeof(jlong) == 8 ? 1 : -1];
  typedef char elixir_jni_md_jint_is_32_bit[sizeof(jint) == 4 ? 1 : -1];
#else /* !_WIN32: upstream definitions */

#ifndef JNIEXPORT
  #if (defined(__GNUC__) && ((__GNUC__ > 4) || (__GNUC__ == 4) && (__GNUC_MINOR__ > 2))) || __has_attribute(visibility)
    #ifdef ARM
      #define JNIEXPORT     __attribute__((externally_visible,visibility("default")))
    #else
      #define JNIEXPORT     __attribute__((visibility("default")))
    #endif
  #else
    #define JNIEXPORT
  #endif
#endif

#if (defined(__GNUC__) && ((__GNUC__ > 4) || (__GNUC__ == 4) && (__GNUC_MINOR__ > 2))) || __has_attribute(visibility)
  #ifdef ARM
    #define JNIIMPORT     __attribute__((externally_visible,visibility("default")))
  #else
    #define JNIIMPORT     __attribute__((visibility("default")))
  #endif
#else
  #define JNIIMPORT
#endif

#define JNICALL

typedef int jint;
#ifdef _LP64
typedef long jlong;
#else
typedef long long jlong;
#endif

typedef signed char jbyte;

#endif /* _WIN32 */

#endif /* !_JAVASOFT_JNI_MD_H_ */
