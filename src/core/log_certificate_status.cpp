/*
SPDX-FileCopyrightText: 2024 Wallix Proxies Team
SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "acl/auth_api.hpp"
#include "core/log_id.hpp"
#include "core/log_certificate_status.hpp"
#include "utils/sugar/zstring_view.hpp"
#include "utils/strutils.hpp"
#include "utils/log.hpp"


void log_certificate_status(
    SessionLogApi& session_log, CertificateStatus status,
    chars_view error_msg, bool verbose,
    ServerCertNotifications notifications)
{
    struct D { ServerCertNotification notification_type; LogId id; zstring_view msg; };

    auto d = [&] {
        switch (status) {
            case CertificateStatus::AccessAllowed: return D{
                notifications.access_allowed_message,
                LogId::CERTIFICATE_CHECK_SUCCESS,
                "Connection to server allowed"_zv,
            };

            case CertificateStatus::CertCreate: return D{
                notifications.create_message,
                LogId::SERVER_CERTIFICATE_NEW,
                "New X.509 certificate created"_zv,
            };

            case CertificateStatus::CertSuccess: return D{
                notifications.success_message,
                LogId::SERVER_CERTIFICATE_MATCH_SUCCESS,
                "X.509 server certificate match"_zv,
            };

            case CertificateStatus::CertFailure: return D{
                notifications.failure_message,
                LogId::SERVER_CERTIFICATE_MATCH_FAILURE,
                "X.509 server certificate match failure"_zv,
            };

            case CertificateStatus::CertError:
                break;
        }

        return D{
            notifications.error_message,
            LogId::SERVER_CERTIFICATE_ERROR,
            "X.509 server certificate internal error: "_zv,
        };
    }();

    if (bool(d.notification_type & ServerCertNotification::SIEM)) {
        chars_view msg = d.msg;
        std::string tmp;
        if (status == CertificateStatus::CertError && !error_msg.empty()) {
            str_assign(tmp, msg, error_msg);
            msg = tmp;
        }
        session_log.log6(d.id, {KVLog("description"_av, msg)});
    }

    LOG_IF(verbose, LOG_INFO, "%s%.*s", d.msg,
        static_cast<int>(error_msg.size()), error_msg.data());
}
