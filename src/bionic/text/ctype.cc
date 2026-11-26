// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
//
// The predicates follow bionic's inline definitions (libc/include/ctype.h,
// BSD-2-Clause, Copyright (C) 2008 The Android Open Source Project). The
// _C_ctype_ table is OpenBSD's libc/gen/ctype_.c as shipped in bionic:
//
//   Copyright (c) 1989 The Regents of the University of California.
//   All rights reserved.
//   (c) UNIX System Laboratories, Inc.
//   Redistribution and use in source and binary forms, with or without
//   modification, are permitted provided that the following conditions
//   are met:
//   1. Redistributions of source code must retain the above copyright
//      notice, this list of conditions and the following disclaimer.
//   2. Redistributions in binary form must reproduce the above copyright
//      notice, this list of conditions and the following disclaimer in the
//      documentation and/or other materials provided with the distribution.
//   3. Neither the name of the University nor the names of its contributors
//      may be used to endorse or promote products derived from this software
//      without specific prior written permission.
//   THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
//   ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
//   IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
//   ARE DISCLAIMED.
#include "bionic/runtime.h"
#include "bionic/text/text_internal.h"

namespace {

namespace ti = elixir::bionic::text_internal;

constexpr char kU = 0x01;
constexpr char kL = 0x02;
constexpr char kN = 0x04;
constexpr char kS = 0x08;
constexpr char kP = 0x10;
constexpr char kC = 0x20;
constexpr char kX = 0x40;
constexpr char kB = static_cast<char>(0x80);

constexpr char kCtypeTable[1 + 256] = {
    0,
    kC, kC, kC, kC, kC, kC, kC, kC,
    kC, kC | kS, kC | kS, kC | kS, kC | kS, kC | kS, kC, kC,
    kC, kC, kC, kC, kC, kC, kC, kC,
    kC, kC, kC, kC, kC, kC, kC, kC,
    static_cast<char>(kS | kB), kP, kP, kP, kP, kP, kP, kP,
    kP, kP, kP, kP, kP, kP, kP, kP,
    kN, kN, kN, kN, kN, kN, kN, kN,
    kN, kN, kP, kP, kP, kP, kP, kP,
    kP, kU | kX, kU | kX, kU | kX, kU | kX, kU | kX, kU | kX, kU,
    kU, kU, kU, kU, kU, kU, kU, kU,
    kU, kU, kU, kU, kU, kU, kU, kU,
    kU, kU, kU, kP, kP, kP, kP, kP,
    kP, kL | kX, kL | kX, kL | kX, kL | kX, kL | kX, kL | kX, kL,
    kL, kL, kL, kL, kL, kL, kL, kL,
    kL, kL, kL, kL, kL, kL, kL, kL,
    kL, kL, kL, kP, kP, kP, kP, kC,
};

}

extern "C" const char* elixir_bionic__ctype_ = kCtypeTable;

ELIXIR_BIONIC_FN(int, isalnum)(int c) noexcept { return ti::IsAlnum(c); }
ELIXIR_BIONIC_FN(int, isalpha)(int c) noexcept { return ti::IsAlpha(c); }
ELIXIR_BIONIC_FN(int, isblank)(int c) noexcept { return ti::IsBlank(c); }
ELIXIR_BIONIC_FN(int, iscntrl)(int c) noexcept { return ti::IsCntrl(c); }
ELIXIR_BIONIC_FN(int, isdigit)(int c) noexcept { return ti::IsDigit(c); }
ELIXIR_BIONIC_FN(int, isgraph)(int c) noexcept { return ti::IsGraph(c); }
ELIXIR_BIONIC_FN(int, islower)(int c) noexcept { return ti::IsLower(c); }
ELIXIR_BIONIC_FN(int, isprint)(int c) noexcept { return ti::IsPrint(c); }
ELIXIR_BIONIC_FN(int, ispunct)(int c) noexcept { return ti::IsPunct(c); }
ELIXIR_BIONIC_FN(int, isspace)(int c) noexcept { return ti::IsSpace(c); }
ELIXIR_BIONIC_FN(int, isupper)(int c) noexcept { return ti::IsUpper(c); }
ELIXIR_BIONIC_FN(int, isxdigit)(int c) noexcept { return ti::IsXdigit(c); }
ELIXIR_BIONIC_FN(int, isascii)(int c) noexcept { return static_cast<unsigned>(c) < 0x80; }
ELIXIR_BIONIC_FN(int, toascii)(int c) noexcept { return c & 0x7f; }
ELIXIR_BIONIC_FN(int, tolower)(int c) noexcept { return ti::ToLower(c); }
ELIXIR_BIONIC_FN(int, toupper)(int c) noexcept { return ti::ToUpper(c); }
ELIXIR_BIONIC_FN(int, _tolower)(int c) noexcept { return c | 0x20; }
ELIXIR_BIONIC_FN(int, _toupper)(int c) noexcept { return c ^ 0x20; }

namespace elixir::bionic::text_internal {

void RegisterCtypeExports(ExportTable& table) {
  table.Add(kLibc, "_ctype_", reinterpret_cast<void*>(&elixir_bionic__ctype_));
  ELIXIR_TEXT_EXPORT(table, kLibc, isalnum);
  ELIXIR_TEXT_EXPORT(table, kLibc, isalpha);
  ELIXIR_TEXT_EXPORT(table, kLibc, isblank);
  ELIXIR_TEXT_EXPORT(table, kLibc, iscntrl);
  ELIXIR_TEXT_EXPORT(table, kLibc, isdigit);
  ELIXIR_TEXT_EXPORT(table, kLibc, isgraph);
  ELIXIR_TEXT_EXPORT(table, kLibc, islower);
  ELIXIR_TEXT_EXPORT(table, kLibc, isprint);
  ELIXIR_TEXT_EXPORT(table, kLibc, ispunct);
  ELIXIR_TEXT_EXPORT(table, kLibc, isspace);
  ELIXIR_TEXT_EXPORT(table, kLibc, isupper);
  ELIXIR_TEXT_EXPORT(table, kLibc, isxdigit);
  ELIXIR_TEXT_EXPORT(table, kLibc, isascii);
  ELIXIR_TEXT_EXPORT(table, kLibc, toascii);
  ELIXIR_TEXT_EXPORT(table, kLibc, tolower);
  ELIXIR_TEXT_EXPORT(table, kLibc, toupper);
  ELIXIR_TEXT_EXPORT(table, kLibc, _tolower);
  ELIXIR_TEXT_EXPORT(table, kLibc, _toupper);
  ELIXIR_TEXT_ALIAS(table, kLibc, isalnum_l, isalnum);
  ELIXIR_TEXT_ALIAS(table, kLibc, isalpha_l, isalpha);
  ELIXIR_TEXT_ALIAS(table, kLibc, isblank_l, isblank);
  ELIXIR_TEXT_ALIAS(table, kLibc, iscntrl_l, iscntrl);
  ELIXIR_TEXT_ALIAS(table, kLibc, isdigit_l, isdigit);
  ELIXIR_TEXT_ALIAS(table, kLibc, isgraph_l, isgraph);
  ELIXIR_TEXT_ALIAS(table, kLibc, islower_l, islower);
  ELIXIR_TEXT_ALIAS(table, kLibc, isprint_l, isprint);
  ELIXIR_TEXT_ALIAS(table, kLibc, ispunct_l, ispunct);
  ELIXIR_TEXT_ALIAS(table, kLibc, isspace_l, isspace);
  ELIXIR_TEXT_ALIAS(table, kLibc, isupper_l, isupper);
  ELIXIR_TEXT_ALIAS(table, kLibc, isxdigit_l, isxdigit);
  ELIXIR_TEXT_ALIAS(table, kLibc, tolower_l, tolower);
  ELIXIR_TEXT_ALIAS(table, kLibc, toupper_l, toupper);
}

}
