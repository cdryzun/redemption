/*
SPDX-FileCopyrightText: 2024 Wallix Proxies Team
SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "utils/sugar/array_view.hpp"

struct TrKey
{
    unsigned index;
};

template<class T> struct TrKeyFmt
{
    unsigned index;
};

template<class T> struct TrKeyPluralFmt
{
    unsigned index;
};
