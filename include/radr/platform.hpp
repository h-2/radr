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

#include <version>

#ifndef __cplusplus
#error "Not a C++ compiler!!"
#endif


// of course libc++ and glibcxx define the FTM only in later versions
#if defined(__cpp_lib_ranges_zip) || \
    ((__cplusplus >= 202002L) && ((defined(_LIBCPP_VERSION) && (_LIBCPP_VERSION >= 170000)) || \
    true)) // GLIBCXX >= 12
#define RADR_COMMON_TUPLE 1
#else
#define RADR_COMMON_TUPLE 0
#endif
