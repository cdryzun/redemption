/*
SPDX-FileCopyrightText: 2025 Wallix Proxies Team
SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "core/WinNT/time.hpp"
#include "core/WinNT/path.hpp"


namespace VNC
{

struct UVncFile
{
    using PathView = WinNtPathView;

    static const unsigned max_path_length = PathView::Bytes::at_most;

    static bool is_path_too_large(size_t len) noexcept
    {
        return is_win_path_too_large(len);
    }

    PathView file_name;
    uint64_t file_size;
    WinNtUTime last_access_time;
    bool is_dir;
};

}
