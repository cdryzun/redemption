/*
SPDX-FileCopyrightText: 2026 Wallix Proxies Team
SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "core/server_cert_params.hpp"
#include "utils/sugar/array_view.hpp"
#include "transport/transport.hpp"  // TlsConfig

struct ModTlsParams
{
    struct ServerCertParams
    {
        bool store = true;
        ServerCertCheck check = ServerCertCheck::fails_if_no_match_or_missing;
        ServerCertNotifications notifications;
    };

    struct CertificateAuthorityParams
    {
        bool enable_ca_certificates = false;
        chars_view certificates = ""_av;
    };

    chars_view device_id;
    chars_view target_host;
    ServerCertParams server_cert;
    CertificateAuthorityParams ca;
    TlsConfig tls_config;
};
