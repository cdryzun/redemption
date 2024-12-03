/*
SPDX-FileCopyrightText: 2024 Wallix Proxies Team
SPDX-License-Identifier: GPL-2.0-or-later
*/

#if defined(IN_IDE_PARSER) && !defined(TR_KV)
#   define TR_KV(name, msg)
#   define TR_KV_FMT(name, msg)
#   define TR_KV_PLURAL_FMT(name, msg, plural_msg)
#   define IN_IDE_PARSER_TR_KV 1
#endif

// widget button
TR_KV(OK, "OK")

// ModuleName::login
TR_KV(optional_target, "Target (optional)")
TR_KV(help_message,
      "The \"Target\" field can be entered with a string labelled in this format:\n"
      "\"Account@Domain@Device:Service:Auth\".\n"
      "The \"Domain\", \"Service\" and \"Auth\" parts are optional.\n"
      "This field is optional and case-sensitive.\n"
      "\n"
      "The \"Login\" field must refer to a user declared on the Bastion.\n"
      "This field is required and not case-sensitive.\n"
      "\n"
      "Contact your system administrator for assistance.")
// ModuleName::login + ModuleName::interactive_target
TR_KV(login, "Login")
TR_KV(password, "Password")
// ModuleName::interactive_target
TR_KV(target_info_required, "Target Information Required")
TR_KV(device, "Device")

// OSD message
TR_KV(disable_osd, "Press \"Insert\" key or left-click to hide this message.")

// OSD (time before closing)
TR_KV_FMT(osd_hour_minute_second_before_closing, "%d hours, %d minutes, %d seconds before closing")
TR_KV_FMT(osd_minute_second_before_closing, "%d minutes, %d seconds before closing")
TR_KV_PLURAL_FMT(osd_second_before_closing, "%d second before closing", "%d seconds before closing")

// OSD (end time warning) + ModuleName::close
TR_KV_PLURAL_FMT(close_box_second_timer, "%d second before closing.", "%d seconds before closing.")
TR_KV_PLURAL_FMT(close_box_minute_timer, "%d minute before closing.", "%d minutes before closing.")
// ModuleName::close (mod_factory)
TR_KV(connection_ended, "Connection to server ended.")
// ModuleName::close
TR_KV(connection_closed, "Connection closed")
TR_KV(close, "Close")
TR_KV(wab_close_username, "Username:")
TR_KV(wab_close_target, "Target:")
TR_KV(wab_close_diagnostic, "Diagnostic:")
TR_KV(wab_close_timeleft, "Time left:")
// ModuleName::close + ModuleName::waitinfo
TR_KV(back_selector, "Back to Selector")
// ModuleName::waitinfo
TR_KV(exit, "Exit")
// ModuleName::waitinfo (form)
TR_KV(comment, "Comment")
TR_KV(comment_r, "Comment *")
TR_KV(ticket, "Ticket Ref.")
TR_KV(ticket_r, "Ticket Ref. *")
TR_KV(duration, "Duration")
TR_KV(duration_r, "Duration *")
TR_KV(note_duration_format, "Format: [hours]h[mins]m")
TR_KV(note_required, "(*) required fields")
TR_KV(confirm, "Confirm")
TR_KV_FMT(fmt_field_required, "Error: %s field is required.")
TR_KV_FMT(fmt_invalid_format, "Error: %s invalid format.")
TR_KV_FMT(fmt_toohigh_duration, "Error: %s is too high (max: %d minutes).")
// ModuleName::waitinfo, ModuleName::valid, ModuleName::confirm (mod_factory)
TR_KV(information, "Information")

// ModuleName::selector
TR_KV(protocol, "Protocol")
TR_KV(authorization, "Authorization")
TR_KV(target, "Target")
TR_KV(logout, "Logout")
TR_KV(filter, "Filter")
TR_KV(connect, "Connect")
TR_KV(no_results, "No results found")
TR_KV(target_accurate_filter_help,
      "Use the following syntax to filter specific fields on a target:\n"
      "    ?<field>=<pattern>\n"
      "where <field> can be \"account\", \"domain\", \"device\" or \"service\",\n"
      "and <pattern> is the value to search for.\n"
      "Use the \"&\" separator to combine several search criteria.\n"
      "Example:\n"
      "    ?account=my_account&?device=my_device")

// TransitionMod
TR_KV(wait_msg, "Please wait...")

// ModuleName::autotest (replay_mod error)
TR_KV(err_replay_open_file, "The recorded file is inaccessible or corrupted.")

// ModuleName::valid (mod_factory)
TR_KV(accepted, "I accept")
TR_KV(refused, "I decline")

// ModuleName::challenge (mod_factory)
TR_KV(challenge, "Challenge")

// ModuleName::link_confirm
TR_KV(link_caption, "URL Redirection")
TR_KV(link_label, "Copy to clipboard: ")
TR_KV(link_copied, "The link is copied.")

// session error
TR_KV(enable_rt_display, "Your session is currently being audited.")
TR_KV(manager_close_cnx, "Connection closed by manager.")
TR_KV(acl_fail, "Authentifier service failed")
// session error (sesman)
TR_KV(err_sesman_unavailable, "No authentifier available.")

// connect_to_target_host error
TR_KV(target_fail, "Failed to connect to remote host.")

// OSD for ICAP with cliprdr
TR_KV(file_verification_wait, "File being analyzed: ")
TR_KV(file_verification_accepted, "Valid file: ")
TR_KV(file_verification_rejected, "Invalid file: ")

// create rdp_mod error
TR_KV(target_shadow_fail, "Failed to connect to remote host. Maybe the session invitation has expired.")
TR_KV(authentification_rdp_fail, "Failed to authenticate with remote RDP host.")
// create vnc_mod error
TR_KV(authentification_vnc_fail, "Failed to authenticate with remote VNC host.")

// mod_rdp error
TR_KV(session_logoff_in_progress, "Session logoff in progress.")
TR_KV(disconnected_by_otherconnection, "Another user connected to the resource, so your connection was lost.")
TR_KV(err_server_denied_connection, "Please check provided Load Balance Info.")
TR_KV(err_mod_rdp_nego, "Fail during TLS security exchange.")
TR_KV(err_mod_rdp_basic_settings_exchange, "Remote access may be denied for the user account.")
TR_KV(err_rdp_channel_connection, "Fail during channels connection.")
TR_KV(err_rdp_get_license, "Failed while trying to get licence.")
TR_KV(err_mod_rdp_connected, "Fail while connecting session on the target.")
TR_KV(err_process_info, "Disconnected by the RDP server.")
// mod_rdp error (RemoteApp)
TR_KV(err_remoteapp_bad_password,
    "(RemoteApp) The logon credentials which were supplied are invalid.")
TR_KV(err_remoteapp_update_password,
    "(RemoteApp) The user cannot continue with the logon process until the password is changed.")
TR_KV(err_remoteapp_failed, "(RemoteApp) The logon process failed.")
TR_KV(err_remoteapp_warning, "(RemoteApp) The logon process has displayed a warning.")
TR_KV(err_remoteapp_unexpected_error, "(RemoteApp) Unexpected Error Notification Type.")
// local error (LocalErrMsg)
TR_KV(err_nla_required, "Fail during TLS security exchange. Enable NLA is probably required.")
TR_KV(err_tls_required, "Fail during TLS security exchange. Enable TLS is probably required.")
TR_KV(session_out_time, "Session is out of allowed timeframe")
TR_KV(miss_keepalive, "Missed keepalive from ACL")
TR_KV(close_inactivity, "Connection closed on inactivity")
TR_KV(err_rdp_server_redir, "The computer that you are trying to connect to is redirecting you to another computer.")
TR_KV(err_nla_authentication_failed, "NLA Authentication Failed.")
// local error (LocalErrMsg) (certificate error)
TR_KV(err_transport_tls_certificate_changed, "TLS certificate changed.")
TR_KV(err_transport_tls_certificate_missed, "TLS certificate missed.")
TR_KV(err_transport_tls_certificate_corrupted, "TLS certificate corrupted.")
TR_KV(err_transport_tls_certificate_inaccessible, "TLS certificate is inaccessible.")
// local error (LocalErrMsg) (certificate error) + ModuleName::close (extra message)
TR_KV(err_transport_tls_certificate_changed_extra_message, "The certificate presented by the target doesn't match the certificate fingerprint stored in the Bastion for this target. Contact your Bastion administrator to delete the existing fingerprint or to change the default behavior of the Bastion when this happens.")
TR_KV(err_transport_tls_certificate_missed_extra_message, "The certificate presented by the target is not stored in the Bastion for this target. Contact your Bastion administrator to add it or to change the behavior of the Bastion when this happens.")
TR_KV(err_transport_tls_certificate_corrupted_extra_message, "The certificate stored in the Bastion for this target is corrupted.")
TR_KV(err_transport_tls_certificate_inaccessible_extra_message, "The certificate stored in the Bastion for this target is inaccessible.")
// mod_rdp error | local error (LocalErrMsg)
TR_KV(end_connection, "End of connection")

// SessionProbe OSD info
TR_KV_FMT(process_interrupted_security_policies, "The process '%.*s' was interrupted in accordance with security policies.")
TR_KV_FMT(account_manipulation_blocked_security_policies, "The account manipulation initiated by process '%.*s' was rejected in accordance with security policies.")

// rail session manager (widget message)
TR_KV(starting_remoteapp, "Starting RemoteApp ...")
TR_KV(closing_remoteapp, "All RemoteApp windows are closed.")
TR_KV(disconnect_now, "Disconnect Now")

// mod_rdp error
TR_KV(err_rdp_unauthorized_user_change, "Unauthorized logon user change detected.")
// local error (LocalErrMsg)
TR_KV(err_vnc_connection_error, "VNC connection error.")
TR_KV(err_rdp_unsupported_monitor_layout, "Unsupported client display monitor layout.")
TR_KV(err_rdp_negotiation, "RDP negotiation phase failure.")
TR_KV(err_rdp_nego_krb_required, "CREDSSP Kerberos Authentication Failed, NTLM not allowed.")
TR_KV(err_rdp_nego_nla_required, "NLA failed. TLS only not allowed.")
TR_KV(err_rdp_nego_nla_restricted_admin, "NLA failed. NLA required by restricted admin mode.")
TR_KV(err_rdp_nego_ssl_required, "Can't activate SSL. RDP Legacy only not allowed")
TR_KV(err_lic, "An error occurred during the licensing protocol.")
TR_KV(err_rail_client_execute, "The RemoteApp program did not start on the remote computer.")
TR_KV(err_rail_starting_program, "Cannot start the RemoteApp program.")
TR_KV(err_rail_unauthorized_program, "The RemoteApp program is not in the list of authorized programs.")
TR_KV(err_rdp_open_session_timeout, "Logon timer expired.")
TR_KV(err_session_probe_launch,"Could not launch Session Probe.")
TR_KV(err_session_probe_asbl_fsvc_unavailable,
      "(ASBL) Could not launch Session Probe. File System Virtual Channel is unavailable. "
      "Please allow the drive redirection in the Remote Desktop Services settings of the target.")
TR_KV(err_session_probe_asbl_maybe_something_blocks,
      "(ASBL) Could not launch Session Probe. Maybe something blocks it on the target. "
      "Is the target running under Microsoft Server products? "
      "The Command Prompt should be published as the RemoteApp program and accept any command-line parameters. "
      "Please also check the temporary directory to ensure there is enough free space.")
TR_KV(err_session_probe_asbl_unknown_reason, "(ASBL) Session Probe launch has failed for unknown reason.")
TR_KV(err_session_probe_cbbl_fsvc_unavailable,
      "(CBBL) Could not launch Session Probe. File System Virtual Channel is unavailable. "
      "Please allow the drive redirection in the Remote Desktop Services settings of the target.")
TR_KV(err_session_probe_cbbl_cbvc_unavailable,
      "(CBBL) Could not launch Session Probe. Clipboard Virtual Channel is unavailable. "
      "Please allow the clipboard redirection in the Remote Desktop Services settings of the target.")
TR_KV(err_session_probe_cbbl_drive_not_ready_yet,
      "(CBBL) Could not launch Session Probe. Drive of Session Probe is not ready yet. "
      "Is the target running under Windows Server 2008 R2 or more recent version?")
TR_KV(err_session_probe_cbbl_maybe_something_blocks,
      "(CBBL) Session Probe is not launched. Maybe something blocks it on the target. "
      "Please also check the temporary directory to ensure there is enough free space.")
TR_KV(err_session_probe_cbbl_launch_cycle_interrupted,
      "(CBBL) Session Probe launch cycle has been interrupted. "
      "The launch timeout duration may be too short.")
TR_KV(err_session_probe_cbbl_unknown_reason_refer_to_syslog,
      "(CBBL) Session Probe launch has failed for unknown reason. "
      "Please refer to the syslog file for more detailed information regarding the error condition.")
TR_KV(err_session_probe_rp_launch_refer_to_syslog,
      "(RP) Could not launch Session Probe. "
      "Please refer to the syslog file for more detailed information regarding the error condition.")
TR_KV(err_session_unknown_backend, "Unknown backend failure.")

#ifdef IN_IDE_PARSER_TR_KV
#   undef TR_KV
#   undef TR_KV_FMT
#   undef IN_IDE_PARSER_TR_KV
#endif
