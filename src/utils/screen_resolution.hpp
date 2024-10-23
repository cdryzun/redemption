/*
SPDX-FileCopyrightText: 2024 Wallix Proxies Team

SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <cstdint>

struct ScreenResolution
{
    uint16_t width;
    uint16_t height;

    bool is_valid() const
    {
        return width && height;
    }
};
