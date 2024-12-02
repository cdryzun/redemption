/*
  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

  Product name: redemption, a FLOSS RDP proxy
  Copyright (C) Wallix 2020
  Author(s): Wallix Team
*/

#include "translation/local_err_msg.hpp"
#include "translation/trkeys.hpp"
#include "core/error.hpp"

LocalErrMsg LocalErrMsg::from_error_id(error_type id) noexcept
{
    REDEMPTION_DIAGNOSTIC_PUSH()
    REDEMPTION_DIAGNOSTIC_GCC_IGNORE("-Wswitch-enum")
    switch (id) {
    case ERR_SESSION_UNKNOWN_BACKEND:
        return {&trkeys::err_session_unknown_backend};

    case ERR_NLA_AUTHENTICATION_FAILED:
        return {&trkeys::err_nla_authentication_failed};

    case ERR_TRANSPORT_TLS_CERTIFICATE_CHANGED:
        return {
            &trkeys::err_transport_tls_certificate_changed,
            &trkeys::err_transport_tls_certificate_changed_extra_message,
        };

    case ERR_TRANSPORT_TLS_CERTIFICATE_MISSED:
        return {
            &trkeys::err_transport_tls_certificate_missed,
            &trkeys::err_transport_tls_certificate_missed_extra_message,
        };

    case ERR_TRANSPORT_TLS_CERTIFICATE_CORRUPTED:
        return {
            &trkeys::err_transport_tls_certificate_corrupted,
            &trkeys::err_transport_tls_certificate_corrupted_extra_message,
        };

    case ERR_TRANSPORT_TLS_CERTIFICATE_INACCESSIBLE:
        return {
            &trkeys::err_transport_tls_certificate_inaccessible,
            &trkeys::err_transport_tls_certificate_inaccessible_extra_message,
        };

    case ERR_VNC_CONNECTION_ERROR:
        return {&trkeys::err_vnc_connection_error};

    case ERR_RDP_UNSUPPORTED_MONITOR_LAYOUT:
        return {&trkeys::err_rdp_unsupported_monitor_layout};

    case ERR_RDP_NEGOTIATION:
        return {&trkeys::err_rdp_negotiation};

    case ERR_NEGO_SSL_REQUIRED_BY_SERVER:
        return {&trkeys::err_tls_required};

    case ERR_NEGO_HYBRID_REQUIRED_BY_SERVER:
        return {&trkeys::err_nla_required};

    case ERR_NEGO_NLA_REQUIRED_BY_RESTRICTED_ADMIN_MODE:
        return {&trkeys::err_rdp_nego_nla_restricted_admin};

    case ERR_NEGO_KRB_REQUIRED:
        return {&trkeys::err_rdp_nego_krb_required};

    case ERR_NEGO_NLA_REQUIRED:
        return {&trkeys::err_rdp_nego_nla_required};

    case ERR_NEGO_SSL_REQUIRED:
        return {&trkeys::err_rdp_nego_ssl_required};

    case ERR_LIC:
        return {&trkeys::err_lic};

    case ERR_RAIL_CLIENT_EXECUTE:
        return {&trkeys::err_rail_client_execute};

    case ERR_RAIL_STARTING_PROGRAM:
        return {&trkeys::err_rail_starting_program};

    case ERR_RAIL_UNAUTHORIZED_PROGRAM:
        return {&trkeys::err_rail_unauthorized_program};

    case ERR_RDP_OPEN_SESSION_TIMEOUT:
        return {&trkeys::err_rdp_open_session_timeout};

    case ERR_RDP_SERVER_REDIR:
        return {&trkeys::err_rdp_server_redir};

    case ERR_SESSION_PROBE_LAUNCH:
        return {&trkeys::err_session_probe_launch};

    case ERR_SESSION_PROBE_ASBL_FSVC_UNAVAILABLE:
        return {&trkeys::err_session_probe_asbl_fsvc_unavailable};

    case ERR_SESSION_PROBE_ASBL_MAYBE_SOMETHING_BLOCKS:
        return {&trkeys::err_session_probe_asbl_maybe_something_blocks};

    case ERR_SESSION_PROBE_ASBL_UNKNOWN_REASON:
        return {&trkeys::err_session_probe_asbl_unknown_reason};

    case ERR_SESSION_PROBE_CBBL_FSVC_UNAVAILABLE:
        return {&trkeys::err_session_probe_cbbl_fsvc_unavailable};

    case ERR_SESSION_PROBE_CBBL_CBVC_UNAVAILABLE:
        return {&trkeys::err_session_probe_cbbl_cbvc_unavailable};

    case ERR_SESSION_PROBE_CBBL_DRIVE_NOT_READY_YET:
        return {&trkeys::err_session_probe_cbbl_drive_not_ready_yet};

    case ERR_SESSION_PROBE_CBBL_MAYBE_SOMETHING_BLOCKS:
        return {&trkeys::err_session_probe_cbbl_maybe_something_blocks};

    case ERR_SESSION_PROBE_CBBL_LAUNCH_CYCLE_INTERRUPTED:
        return {&trkeys::err_session_probe_cbbl_launch_cycle_interrupted};

    case ERR_SESSION_PROBE_CBBL_UNKNOWN_REASON_REFER_TO_SYSLOG:
        return {&trkeys::err_session_probe_cbbl_unknown_reason_refer_to_syslog};

    case ERR_SESSION_PROBE_RP_LAUNCH_REFER_TO_SYSLOG:
        return {&trkeys::err_session_probe_rp_launch_refer_to_syslog};

    case ERR_SESSION_CLOSE_ENDDATE_REACHED:
        return {&trkeys::session_out_time};

    case ERR_MCS_APPID_IS_MCS_DPUM:
        return {&trkeys::end_connection};

    case ERR_SESSION_CLOSE_ACL_KEEPALIVE_MISSED:
        return {&trkeys::miss_keepalive};

    case ERR_SESSION_CLOSE_USER_INACTIVITY:
        return {&trkeys::close_inactivity};

    default:
        return {};
    }
    REDEMPTION_DIAGNOSTIC_POP()
}
