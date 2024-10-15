/*
SPDX-FileCopyrightText: 2024 Wallix Proxies Team
SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "core/server_cert_params.hpp"
#include "configs/config.hpp"

inline ServerCertParams load_server_cert_params(Inifile const& ini)
{
    return ServerCertParams{
        .store = ini.get<cfg::server_cert::server_cert_store>(),
        .check = ini.get<cfg::server_cert::server_cert_check>(),
        .notifications = {
            .access_allowed_message = ini.get<cfg::server_cert::server_access_allowed_message>(),
            .create_message = ini.get<cfg::server_cert::server_cert_create_message>(),
            .success_message = ini.get<cfg::server_cert::server_cert_success_message>(),
            .failure_message = ini.get<cfg::server_cert::server_cert_failure_message>(),
            .error_message = ini.get<cfg::server_cert::error_message>(),
        },
    };
}
