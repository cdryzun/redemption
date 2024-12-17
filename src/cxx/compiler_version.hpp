/*
SPDX-FileCopyrightText: 2024 Wallix Proxies Team
SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "utils/pp.hpp"

// cuda
#if defined(__CUDACC__)
#   define REDEMPTION_COMP_CUDA (__CUDACC_VER_MAJOR__ * 100 + __CUDACC_VER_MINOR__)
#   if defined(__clang__)
#       define REDEMPTION_HOST_COMP_CLANG (__clang_major__ * 100 + __clang_minor__)
#   elif defined(__GNUC__)
#       define REDEMPTION_HOST_COMP_GCC (__GNUC__ * 100 + __GNUC_MINOR__)
#   endif
#   define REDEMPTION_COMP_NAME cuda

// clang-cl
#elif defined(_MSC_VER) && defined(__clang__)
#   define REDEMPTION_COMP_MSVC_LIKE _MSC_VER
#   define REDEMPTION_COMP_CLANG_CL (__clang_major__ * 100 + __clang_minor__)
#   define REDEMPTION_COMP_NAME clang_cl

// clang
#elif defined(__clang__)
#   define REDEMPTION_COMP_CLANG (__clang_major__ * 100 + __clang_minor__)
#   define REDEMPTION_COMP_NAME clang

// msvc
#elif defined(_MSC_VER)
#   define REDEMPTION_COMP_MSVC _MSC_VER
#   define REDEMPTION_COMP_MSVC_LIKE _MSC_VER
#   define REDEMPTION_COMP_NAME msvc

// gcc
#elif defined(__GNUC__)
#   define REDEMPTION_COMP_GCC (__GNUC__ * 100 + __GNUC_MINOR__)
#   define REDEMPTION_COMP_NAME gcc

// other
#else
#   define REDEMPTION_COMP_NAME unknown
#endif


#if defined(__clang__)
#   define REDEMPTION_COMP_CLANG_LIKE (__clang_major__ * 100 + __clang_minor__)
#else
#   define REDEMPTION_COMP_CLANG_LIKE 0
#endif

#if defined(__APPLE__) && REDEMPTION_COMP_CLANG
#   define REDEMPTION_COMP_APPLE_CLANG REDEMPTION_COMP_CLANG
#else
#   define REDEMPTION_COMP_APPLE_CLANG 0
#endif

#ifndef REDEMPTION_COMP_CUDA
#   define REDEMPTION_COMP_CUDA 0
#endif

#ifndef REDEMPTION_COMP_MSVC
#   define REDEMPTION_COMP_MSVC 0
#endif

#ifndef REDEMPTION_COMP_MSVC_LIKE
#   define REDEMPTION_COMP_MSVC_LIKE 0
#endif

#ifndef REDEMPTION_COMP_CLANG
#   define REDEMPTION_COMP_CLANG 0
#endif

#ifndef REDEMPTION_COMP_CLANG_CL
#   define REDEMPTION_COMP_CLANG_CL 0
#endif

#ifndef REDEMPTION_COMP_GCC
#   define REDEMPTION_COMP_GCC 0
#endif


#define REDEMPTION_WORKAROUND(symbol, test) ((symbol) != 0 && ((symbol) test))


#if defined(__clang__)
# define REDEMPTION_COMP_STRING_VERSION   \
    RED_PP_STRINGIFY(__clang_major__) "." \
    RED_PP_STRINGIFY(__clang_minor__) "." \
    RED_PP_STRINGIFY(__clang_patchlevel__)
#elif defined(__GNUC__)
# ifdef __GNUC_PATCHLEVEL__
#   define REDEMPTION_COMP_STRING_VERSION  \
      RED_PP_STRINGIFY(__GNUC__) "."       \
      RED_PP_STRINGIFY(__GNUC_MINOR__) "." \
      RED_PP_STRINGIFY(__GNUC_PATCHLEVEL__)
# else
#   define REDEMPTION_COMP_STRING_VERSION \
      RED_PP_STRINGIFY(__GNUC__) "."      \
      RED_PP_STRINGIFY(__GNUC_MINOR__) ".0"
# endif
#elif defined(_MSC_VER)
# define REDEMPTION_COMP_STRING_VERSION RED_PP_STRINGIFY(_MSC_VER)
#else
# define REDEMPTION_COMP_STRING_VERSION ""
#endif
