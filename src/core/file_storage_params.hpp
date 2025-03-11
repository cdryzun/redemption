/*
SPDX-FileCopyrightText: 2026 Wallix Proxies Team
SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

class FdxCapture;

struct FileStorageParams
{
    FdxCapture * fdx_capture;

    bool always_file_storage;

    explicit operator bool () const noexcept
    {
        return fdx_capture;
    }
};

