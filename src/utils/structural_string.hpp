/*
SPDX-FileCopyrightText: 2025 Wallix Proxies Team
SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <utility>


template<std::size_t N>
struct structural_string
{
  char _buffer[N] {};

  constexpr structural_string(char const(&str)[N]) noexcept
    : structural_string(str, std::make_index_sequence<N-1>{})
  {}

  constexpr char const * data() const noexcept { return _buffer; }
  constexpr std::size_t size() const noexcept { return N-1; }

  constexpr bool is_empty() const noexcept { return N == 1; }

  constexpr char const * c_str() const noexcept { return _buffer; }

  constexpr char const * begin() const noexcept { return _buffer; }
  constexpr char const * end() const noexcept { return _buffer - 1; }

private:
  template<std::size_t... i>
  constexpr structural_string(char const(&str)[N], std::index_sequence<i...>) noexcept
    : _buffer{str[i]...}
  {}
};

template<std::size_t N>
structural_string(char const(&)[N]) -> structural_string<N>;
