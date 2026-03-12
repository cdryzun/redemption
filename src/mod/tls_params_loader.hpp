/*
SPDX-FileCopyrightText: 2026 Wallix Proxies Team
SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "mod/tls_params.hpp"
#include "configs/config.hpp"

struct ModTlsParamsLoader
{
    static ModTlsParams rdp(Inifile const& ini)
    {
        return load(ini, true);
    }

    static ModTlsParams vnc(Inifile const& ini)
    {
        return load(ini, false);
    }

private:
    template<class cfg_tls>
    static TlsConfig load_tls_config(Inifile const& ini)
    {
        return TlsConfig {
            .min_level = ini.get<typename cfg_tls::tls_min_level>(),
            .max_level = ini.get<typename cfg_tls::tls_max_level>(),
            .cipher_list = ini.get<typename cfg_tls::cipher_string>(),
            .tls_1_3_ciphersuites = ini.get<typename cfg_tls::tls_1_3_ciphersuites>(),
            .key_exchange_groups = ini.get<typename cfg_tls::tls_key_exchange_groups>(),
            .signature_algorithms = ini.get<typename cfg_tls::tls_signature_algorithms>(),
            .enable_legacy_server_connect = ini.get<typename cfg_tls::tls_enable_legacy_server>(),
            .show_common_cipher_list = ini.get<typename cfg_tls::show_common_cipher_list>(),
        };
    }

    static ModTlsParams load(Inifile const& ini, bool is_rdp)
    {
        bool use_ca = ini.get<cfg::server_cert::server_cert_check_using_ca>();

        return ModTlsParams {
            .device_id = ini.get<cfg::globals::device_id>(),
            .target_host = ini.get<cfg::context::target_host>(),
            .server_cert {
                .store = ini.get<cfg::server_cert::server_cert_store>(),
                .check = ini.get<cfg::server_cert::server_cert_check>(),
                .notifications = {
                    .access_allowed_message = ini.get<cfg::server_cert::server_access_allowed_message>(),
                    .create_message = ini.get<cfg::server_cert::server_cert_create_message>(),
                    .success_message = ini.get<cfg::server_cert::server_cert_success_message>(),
                    .failure_message = ini.get<cfg::server_cert::server_cert_failure_message>(),
                    .error_message = ini.get<cfg::server_cert::error_message>(),
                    .not_trusted_message = ServerCertNotification::SIEM,
                    .trusted_message = ServerCertNotification::SIEM,
                },
            },
            .ca {
                .enable_ca_certificates = use_ca,
                .certificates = use_ca ? ini.get<cfg::context::ca_certificates>() : chars_view{},
            },
            .tls_config = is_rdp
                ? load_tls_config<cfg::mod_rdp>(ini)
                : load_tls_config<cfg::mod_vnc>(ini),
        };
    }
};
