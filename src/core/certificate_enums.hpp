/*
SPDX-FileCopyrightText: 2024 Wallix Proxies Team
SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

enum class CertificateResult : uint8_t
{
    Invalid,
    Valid,
    Wait,
};

enum class CertificateStatus : uint8_t
{
    AccessAllowed,
    CertCreate,
    CertSuccess,
    CertFailure,
    CertError,
};
