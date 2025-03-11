/*
SPDX-FileCopyrightText: 2025 Wallix Proxies Team
SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "utils/sugar/bytes_view.hpp"

#include <cstring>


inline bool bytes_equal(bytes_view a, bytes_view b) noexcept
{
    return a.size() == b.size()
        && 0 == memcmp(a.data(), b.data(), a.size());
}
