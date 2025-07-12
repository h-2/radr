// -*- C++ -*-
//===----------------------------------------------------------------------===//
//
// Copyright (c) 2023-2025 Hannes Hauswedell
//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See the LICENSE file for details.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#pragma once

#include <cinttypes>
#include <ciso646> // makes _LIBCPP_VERSION available
#include <cstddef> // makes __GLIBCXX__ available

// ============================================================================
//  C++ standard and features
// ============================================================================

// C++ standard [required]
#ifdef __cplusplus
static_assert(__cplusplus >= 201709, "RADR requires C++20 or later, make sure that you have set -std=c++20.");
#else
#    error "This is not a C++ compiler."
#endif

#if __has_include(<version>)
#    include <version>
#endif

// ============================================================================
//  Compilers
// ============================================================================

#if !(defined(__clang_major__) && (__clang_major__ >= 16)) && !(defined(__GNUC__) && (__GNUC__ >= 11))
#    pragma GCC warning "Only Clang >= 16 and GCC >= 11 are officially supported."
#endif

// ============================================================================
//  Version
// ============================================================================

#define RADR_VERSION_MAJOR 0
#define RADR_VERSION_MINOR 20
#define RADR_VERSION_PATCH 0
