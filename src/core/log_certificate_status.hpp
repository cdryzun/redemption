/*
SPDX-FileCopyrightText: 2024 Wallix Proxies Team
SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "utils/sugar/array_view.hpp"
#include "core/certificate_enums.hpp"
#include "core/server_cert_params.hpp"

class SessionLogApi;

void log_certificate_status(
    SessionLogApi& session_log, CertificateStatus status,
    chars_view error_msg, bool verbose,
    ServerCertNotifications notifications
);
