/*
SPDX-FileCopyrightText: 2024 Wallix Proxies Team
SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "utils/sugar/noncopyable.hpp"
#include "utils/sugar/array_view.hpp"
#include "utils/pp.hpp"

// f(id, funcname, param_or_msg)
#define REDEMPTION_ACL_REPORT_DEF(f)                                           \
    f(FILESYSTEM_FULL, file_system_full, MSG(""))                              \
    f(FINDCONNECTION_DENY, find_connection_deny, PARAM())                      \
    f(FINDCONNECTION_NOTIFY, find_connection_notify, PARAM())                  \
    f(FINDPATTERN_NOTIFY, find_pattern_notify, PARAM())                        \
    f(FINDPATTERN_KILL, find_pattern_kill, PARAM())                            \
    f(FINDPROCESS_DENY, find_process_deny, PARAM())                            \
    f(FINDPROCESS_NOTIFY, find_process_notify, PARAM())                        \
    f(CLOSE_SESSION_SUCCESSFUL, close_session_successful, MSG("OK."))          \
    f(CONNECT_DEVICE_SUCCESSFUL, connect_device_successful, MSG("OK."))        \
    f(CONNECTION_FAILED, connection_failed, PARAM())                           \
    f(OPEN_SESSION_FAILED, open_session_failed, PARAM())                       \
    f(OPEN_SESSION_SUCCESSFUL, open_session_successful, MSG("OK."))            \
    f(SERVER_REDIRECTION, server_redirection, PARAM())                         \
    f(SESSION_EVENT, session_event, PARAM())                                   \
    f(SESSION_EXCEPTION, session_exception, PARAM())                           \
    f(SESSION_EXCEPTION_NO_RECORD, session_exception_no_record, PARAM())       \
    f(SESSION_PROBE_KEEPALIVE_MISSED, session_probe_keepalive_missed, MSG("")) \
    f(SESSION_PROBE_LAUNCH_FAILED, session_probe_launch_failed, PARAM())       \
    f(SESSION_PROBE_OUTBOUND_CONNECTION_BLOCKING_FAILED,                       \
        session_probe_outbound_connection_blocking_failed, MSG(""))            \
    f(SESSION_PROBE_PROCESS_BLOCKING_FAILED,                                   \
        session_probe_process_blocking_failed, MSG(""))                        \
    f(SESSION_PROBE_RUN_STARTUP_APPLICATION_FAILED,                            \
        session_probe_run_startup_application_failed, PARAM())                 \
    f(SESSION_PROBE_RECONNECTION, session_probe_reconnection, MSG(""))

struct AclReport {
    #define REDEMPTION_ACL_REPORT_ID(id, func, msg) id,
    enum class ReasonId {
        REDEMPTION_ACL_REPORT_DEF(REDEMPTION_ACL_REPORT_ID)
    };
    #undef REDEMPTION_ACL_REPORT_ID

    #define REDEMPTION_ACL_REPORT_SIG_PARAM() (chars_view message)
    #define REDEMPTION_ACL_REPORT_VAR_PARAM() message
    #define REDEMPTION_ACL_REPORT_SIG_MSG(msg) ()
    #define REDEMPTION_ACL_REPORT_VAR_MSG(msg) msg ""_av
    #define REDEMPTION_ACL_REPORT_FUNC(id, func, msg)                                    \
        static AclReport func RED_PP_CAT(REDEMPTION_ACL_REPORT_SIG_, msg) noexcept {     \
            return AclReport(ReasonId::id, RED_PP_CAT(REDEMPTION_ACL_REPORT_VAR_, msg)); \
        }
    REDEMPTION_ACL_REPORT_DEF(REDEMPTION_ACL_REPORT_FUNC)
    #undef REDEMPTION_ACL_REPORT_FUNC
    #undef REDEMPTION_ACL_REPORT_SIG_MSG
    #undef REDEMPTION_ACL_REPORT_VAR_MSG
    #undef REDEMPTION_ACL_REPORT_SIG_PARAM
    #undef REDEMPTION_ACL_REPORT_VAR_PARAM

    chars_view reason() const noexcept
    {
        #define REDEMPTION_ACL_REPORT_CASE(id, func, msg) case ReasonId::id: return #id ""_av;
        switch (reason_id_) {
            REDEMPTION_ACL_REPORT_DEF(REDEMPTION_ACL_REPORT_CASE)
        }
        #undef REDEMPTION_ACL_REPORT_CASE
        return {};
    }

    /// Indicates whether the construction function associated
    /// with \c ReasonId required a parameter.
    /// Not to be confused with `message().empty()` which is not
    /// true when a message is hardwritten by the construct function.
    bool init_with_user_message() const noexcept
    {
        #define REDEMPTION_ACL_REPORT_MASK_PARAM() ~0u
        #define REDEMPTION_ACL_REPORT_MASK_MSG(msg) 0u
        #define REDEMPTION_ACL_REPORT_MASK(id, func, msg)  \
            | ((1u << static_cast<unsigned>(ReasonId::id)) \
               & RED_PP_CAT(REDEMPTION_ACL_REPORT_MASK_, msg))
        return (1u << static_cast<unsigned>(reason_id_)) & (
            0 REDEMPTION_ACL_REPORT_DEF(REDEMPTION_ACL_REPORT_MASK)
        );
        #undef REDEMPTION_ACL_REPORT_MASK
        #undef REDEMPTION_ACL_REPORT_MASK_MSG
        #undef REDEMPTION_ACL_REPORT_MASK_PARAM
    }

    ReasonId reason_id() const noexcept { return reason_id_; }
    chars_view message() const noexcept { return message_; }

private:
    AclReport(ReasonId reason, chars_view message) noexcept
      : reason_id_(reason)
      , message_(message)
    {}

    ReasonId reason_id_;
    chars_view message_;
};

#undef REDEMPTION_ACL_REPORT_DEF
