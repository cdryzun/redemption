/*
SPDX-FileCopyrightText: 2024 Wallix Proxies Team
SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "configs/config.hpp"
#include "mod/rdp/rdp_params.hpp"
#include "utils/ascii.hpp"
#include "utils/fileutils.hpp"
#include "utils/strutils.hpp"

inline void update_application_driver(ModRDPParams & mod_rdp_params, Inifile & ini)
{
    std::string_view application_driver_exe_or_file;
    std::string_view application_driver_script;
    std::string_view application_driver_script_argument_extra = "";

    auto upper = ascii_to_limited_upper<64>(mod_rdp_params.application_params.alternate_shell);

    if (upper == "__APP_DRIVER_IE__"_ascii_upper) {
        application_driver_exe_or_file           = ini.get<cfg::mod_rdp::application_driver_exe_or_file>();
        application_driver_script                = ini.get<cfg::mod_rdp::application_driver_ie_script>();
    }
    else if (upper == "__APP_DRIVER_CHROME_DT__"_ascii_upper) {
        application_driver_exe_or_file           = ini.get<cfg::mod_rdp::application_driver_exe_or_file>();
        application_driver_script                = ini.get<cfg::mod_rdp::application_driver_chrome_dt_script>();
    }
    else if (upper == "__APP_DRIVER_CHROME_UIA__"_ascii_upper) {
        application_driver_exe_or_file           = ini.get<cfg::mod_rdp::application_driver_exe_or_file>();
        application_driver_script                = ini.get<cfg::mod_rdp::application_driver_chrome_uia_script>();
    }
    else if (upper == "__APP_DRIVER_EDGE_CHROMIUM_DT__"_ascii_upper) {
        application_driver_exe_or_file           = ini.get<cfg::mod_rdp::application_driver_exe_or_file>();
        application_driver_script                = ini.get<cfg::mod_rdp::application_driver_chrome_dt_script>();
        application_driver_script_argument_extra = " /e:UseEdgeChromium=Yes";
    }
    else if (upper == "__APP_DRIVER_EDGE_CHROMIUM_UIA__"_ascii_upper) {
        application_driver_exe_or_file           = ini.get<cfg::mod_rdp::application_driver_exe_or_file>();
        application_driver_script                = ini.get<cfg::mod_rdp::application_driver_chrome_uia_script>();
        application_driver_script_argument_extra = " /e:UseEdgeChromium=Yes";
    }
    else if (upper == "__APP_DRIVER_FIREFOX_UIA__"_ascii_upper) {
        application_driver_exe_or_file           = ini.get<cfg::mod_rdp::application_driver_exe_or_file>();
        application_driver_script                = ini.get<cfg::mod_rdp::application_driver_firefox_uia_script>();
    }
    else {
        return;
    }

    std::string_view application_driver_prefix_argument_channel_extra = "";
    std::string_view application_driver_argument_channel_extra = "";
    if (CHANNELS::ChannelNameId("wablnch") != mod_rdp_params.auth_channel) {
        application_driver_prefix_argument_channel_extra = " /v:";
        application_driver_argument_channel_extra = mod_rdp_params.auth_channel.c_str();
    }

    std::string & application_driver_alternate_shell = mod_rdp_params.application_params.alternate_shell;
    application_driver_alternate_shell.clear();
    application_driver_alternate_shell.reserve(512);

    std::string file_contents;
    std::string file_path;
    auto drive_redirection = app_path(AppPath::DriveRedirection);

    for (auto filename : {application_driver_exe_or_file, application_driver_script}) {
        str_append(application_driver_alternate_shell, "\x02\\\\tsclient\\SESPRO\\"_av, filename);

        str_assign(file_path, drive_redirection, "/sespro/", filename, ".hash");
        file_contents.clear();
        (void)append_file_contents(file_path, file_contents, 8192);

        size_t pos = file_contents.find(' ');

        if (std::string::npos != pos) {
            str_append(
                application_driver_alternate_shell,
                '\x02', chars_view{file_contents}.first(pos),
                '\x02', chars_view{file_contents}.drop_front(pos+1)
            );
        }
        else {
            application_driver_alternate_shell += "\x02\x02";
        }
    }

    application_driver_alternate_shell += "\x02";

    str_assign(mod_rdp_params.application_params.shell_arguments,
        ini.get<cfg::mod_rdp::application_driver_script_argument>(),
        application_driver_script_argument_extra,
        application_driver_prefix_argument_channel_extra,
        application_driver_argument_channel_extra,
        ' ',
        ini.get<cfg::mod_rdp::shell_arguments>()
    );

    mod_rdp_params.session_probe_params.enable_session_probe                               = true;
    mod_rdp_params.session_probe_params.vc_params.launch_application_driver                = true;
    mod_rdp_params.session_probe_params.vc_params.launch_application_driver_then_terminate = !(ini.get<cfg::session_probe::enable_session_probe>());

    mod_rdp_params.session_probe_params.vc_params.on_launch_failure                        = SessionProbeOnLaunchFailure::disconnect_user;
    ini.set<cfg::session_probe::on_launch_failure>(SessionProbeOnLaunchFailure::disconnect_user);

    if (!ini.get<cfg::session_probe::enable_session_probe>())
    {
        mod_rdp_params.session_probe_params.vc_params.on_keepalive_timeout = SessionProbeOnKeepaliveTimeout::ignore_and_continue;
        ini.set<cfg::session_probe::on_keepalive_timeout>(SessionProbeOnKeepaliveTimeout::ignore_and_continue);
    }

    if (ini.get<cfg::session_probe::enable_autodeployed_appdriver_affinity>()) {
        mod_rdp_params.session_probe_params.vc_params.end_disconnected_session = true;
    }
}
