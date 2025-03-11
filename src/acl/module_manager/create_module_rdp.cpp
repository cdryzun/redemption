/*
SPDX-FileCopyrightText: 2026 Wallix Proxies Team
SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "core/auth_channel_name.hpp"
#include "core/file_validator/file_validator.hpp"
#include "core/file_storage.hpp"
#include "utils/sugar/unique_fd.hpp"
#include "utils/sugar/bytes_view.hpp"
#include "utils/error_message_ctx.hpp"
#include "utils/netutils.hpp"
#include "utils/ascii.hpp"
#include "utils/parse_primary_drawing_orders.hpp"
#include "mod/tls_params_loader.hpp"
#include "mod/rdp/params/rdp_session_probe_params.hpp"
#include "mod/rdp/params/rdp_application_params.hpp"
#include "mod/rdp/rdp.hpp"
#include "mod/rdp/rdp_params.hpp"
#include "mod/rdp/rdp_verbose.hpp"
#include "mod/internal/rail_module_host_mod.hpp"
#include "acl/module_manager/create_module_rdp.hpp"
#include "acl/module_manager/create_module_rail.hpp"
#include "acl/module_manager/update_application_driver.hpp"
#include "acl/connect_to_target_host.hpp"
#include "transport/failure_simulation_socket_transport.hpp"
#include "transport/socket_transport.hpp"


namespace
{

// READ PROXY_OPT
ChannelsAuthorizations make_channels_authorizations(Inifile const& ini)
{
    auto const& allow = ini.get<cfg::mod_rdp::allowed_channels>();
    auto const& deny = ini.get<cfg::mod_rdp::denied_channels>();

    if (ini.get<cfg::globals::enable_wab_integration>()) {
        auto result = compute_authorized_channels(allow, deny, ini.get<cfg::context::proxy_opt>());
        return ChannelsAuthorizations(result.first, result.second);
    }
    return ChannelsAuthorizations(allow, deny);
}

RDPVerbose get_rdp_verbose(Inifile const & ini) noexcept
{
    return safe_cast<RDPVerbose>(ini.get<cfg::debug::mod_rdp>());
}

struct RdpSocket
{
public:
    RdpSocket(
        Inifile & ini
      , SocketTransport::Name name
      , unique_fd sck
      , SocketTransport::Verbose verbose
      , ModRdpUseFailureSimulationSocketTransport use_failure_simulation_socket_transport
    )
    : socket_transport_ptr(
        [&]() -> SocketTransport* {
            chars_view ip_address = ini.get<cfg::context::target_host>();
            int port = checked_int(ini.get<cfg::context::target_port>());
            auto recv_timeout = std::chrono::milliseconds(ini.get<cfg::globals::mod_recv_timeout>());
            auto connection_establishment_timeout
                = ini.get<cfg::all_target_mod::connection_establishment_timeout>();
            auto tcp_user_timeout = ini.get<cfg::all_target_mod::tcp_user_timeout>();

            if (ModRdpUseFailureSimulationSocketTransport::Off == use_failure_simulation_socket_transport) {
                return new SocketTransport( /*NOLINT*/
                    name,
                    std::move(sck),
                    ip_address,
                    port,
                    connection_establishment_timeout,
                    tcp_user_timeout,
                    recv_timeout,
                    verbose
                );
            }

            const bool is_read_error_simulation
                = ModRdpUseFailureSimulationSocketTransport::SimulateErrorRead == use_failure_simulation_socket_transport;
            LOG(LOG_WARNING, "ModRDPWithSocket::ModRDPWithSocket: Mod_rdp use Failure Simulation Socket Transport (mode=%s)",
                is_read_error_simulation ? "SimulateErrorRead" : "SimulateErrorWrite");

            return new FailureSimulationSocketTransport( /*NOLINT*/
                is_read_error_simulation,
                name,
                std::move(sck),
                ip_address,
                port,
                connection_establishment_timeout,
                tcp_user_timeout,
                recv_timeout,
                verbose
            );
        }()
    )
    {}

    ~RdpSocket() = default;

    SocketTransport& get_transport() const
    {
        return *this->socket_transport_ptr;
    }

private:
    std::unique_ptr<SocketTransport> socket_transport_ptr;
};


class ModRDPWithSocket final : public RdpSocket, public mod_rdp
{
    FileValidator file_validator_ctx;
    FileStorage file_storage_ctx;
    Random & gen;
    Inifile & ini;
    CryptoContext & cctx;
    SessionLogApi & session_log;
    EventsGuard events_guard;

public:
    ModRDPWithSocket(
        gdi::OsdApi & osd
      , Inifile & ini
      , SocketTransport::Name name
      , unique_fd sck
      , SocketTransport::Verbose verbose
      , EventContainer & events
      , SessionLogApi& session_log
      , ErrorMessageCtx& err_msg_ctx
      , gdi::GraphicApi & gd
      , FrontAPI & front
      , const ClientInfo & info
      , RedirectionInfo & redir_info
      , Random & gen
      , CryptoContext & cctx
      , const ChannelsAuthorizations & channels_authorizations
      , ModRDPParams & mod_rdp_params
      , LicenseApi & license_store
      , ModRdpVariables vars
      , ModRdpUseFailureSimulationSocketTransport use_failure_simulation_socket_transport
      , TransportWrapperFnView& transport_wrapper_fn
    )
    : RdpSocket(ini, name, std::move(sck), verbose, use_failure_simulation_socket_transport)
    , mod_rdp(
        transport_wrapper_fn(this->get_transport()),
        gd,
        osd,
        events,
        session_log,
        err_msg_ctx,
        front,
        info,
        redir_info,
        gen,
        channels_authorizations,
        [&] () -> ModRDPParams const & {
            mod_rdp_params.get_clipboard_params
                = [this](ChannelsAuthorizations const & channels_authorizations){
                    return get_clipboard_params(channels_authorizations);
                };
            return mod_rdp_params;
        }(),
        license_store,
        vars
    )
    , gen(gen)
    , ini(ini)
    , cctx(cctx)
    , session_log(session_log)
    , events_guard(events)
    {}

private:
    ClipboardVirtualChannelParams
    get_clipboard_params(ChannelsAuthorizations const & channels_authorizations)
    {
        bool clipboard_up_authorized = channels_authorizations.cliprdr_up_is_authorized();
        bool clipboard_down_authorized = channels_authorizations.cliprdr_down_is_authorized();
        bool file_authorized = channels_authorizations.cliprdr_file_is_authorized();

        FileValidator::Params validators_params {
            .up = {
                .text_authorized = clipboard_up_authorized,
                .file_authorized = clipboard_up_authorized && file_authorized,
            },
            .down = {
                .text_authorized = clipboard_down_authorized,
                .file_authorized = clipboard_down_authorized && file_authorized,
            },
            .verbose_validator = bool(get_rdp_verbose(ini) & RDPVerbose::cliprdr),
        };

        auto file_validator = file_validator_ctx.init(
            ini,
            session_log,
            validators_params,
            events_guard,
            [this](Event& /*event*/)
            {
                DLP_antivirus_check_channels_files();
            }
        );

        return ClipboardVirtualChannelParams {
            .cliprdr_params {
                .download_authorized = clipboard_down_authorized,
                .upload_authorized   = clipboard_up_authorized,
                .file_authorized     = file_authorized,
                .log_only_relevant_activities
                    = ini.get<cfg::mod_rdp::log_only_relevant_clipboard_activities>(),
                .log_text = ini.get<cfg::mod_rdp::log_clipboard_text>(),
            },
            .validator_params {
                .file = file_validator,
                .text = {
                    .enable_text_upload = ini.get<cfg::file_verification::clipboard_text_up>(),
                    .enable_text_download = ini.get<cfg::file_verification::clipboard_text_down>(),
                    .block_invalid_text_upload
                        = ini.get<cfg::file_verification::block_invalid_clipboard_text_up>(),
                    .block_invalid_text_download
                        = ini.get<cfg::file_verification::block_invalid_clipboard_text_down>(),
                },
                .osd_delay = std::chrono::seconds(5),
            },
            .file_storage_params = file_storage_ctx.init(
                gen, ini, cctx, session_log,
                {
                    .up_file_authorized = validators_params.up.file_authorized,
                    .down_file_authorized = validators_params.down.file_authorized,
                }
            ),
        };
    }
};

inline static ModRdpSessionProbeParams get_session_probe_params(Inifile & ini)
{
    ModRdpSessionProbeParams spp;
    spp.enable_session_probe = ini.get<cfg::session_probe::enable_session_probe>();
    spp.enable_launch_mask = ini.get<cfg::session_probe::enable_launch_mask>();
    spp.used_clipboard_based_launcher = ini.get<cfg::session_probe::use_smart_launcher>();
    spp.ensure_launch_sequence_only_starts_after_logon =
        ini.get<cfg::session_probe::ensure_launch_sequence_only_starts_after_logon>();
    spp.start_launch_timeout_timer_only_after_logon = ini.get<cfg::session_probe::start_launch_timeout_timer_only_after_logon>();
    spp.vc_params.effective_launch_timeout
        = ini.get<cfg::session_probe::start_launch_timeout_timer_only_after_logon>()
        ? ((ini.get<cfg::session_probe::on_launch_failure>()
                == SessionProbeOnLaunchFailure::disconnect_user)
            ? ini.get<cfg::session_probe::launch_timeout>()
            : ini.get<cfg::session_probe::launch_fallback_timeout>())
        : std::chrono::milliseconds::zero();
    spp.vc_params.on_launch_failure = ini.get<cfg::session_probe::on_launch_failure>();
    spp.vc_params.keepalive_timeout = ini.get<cfg::session_probe::keepalive_timeout>();
    spp.vc_params.on_keepalive_timeout  =
        ini.get<cfg::session_probe::on_keepalive_timeout>();
    spp.vc_params.end_disconnected_session =
        ini.get<cfg::session_probe::end_disconnected_session>();
    spp.customize_executable_name =
        ini.get<cfg::session_probe::customize_executable_name>();
    spp.vc_params.disconnected_application_limit =
        ini.get<cfg::session_probe::disconnected_application_limit>();
    spp.vc_params.disconnected_session_limit =
        ini.get<cfg::session_probe::disconnected_session_limit>();
    spp.vc_params.idle_session_limit =
        ini.get<cfg::session_probe::idle_session_limit>();
    spp.exe_or_file = ini.get<cfg::session_probe::exe_or_file>();
    spp.arguments = ini.get<cfg::session_probe::arguments>();
    spp.vc_params.launcher_abort_delay =
        ini.get<cfg::session_probe::launcher_abort_delay>();
    spp.clipboard_based_launcher.clipboard_initialization_delay_ms =
        ini.get<cfg::session_probe::smart_launcher_clipboard_initialization_delay>();
    spp.clipboard_based_launcher.start_delay_ms =
        ini.get<cfg::session_probe::smart_launcher_start_delay>();
    spp.clipboard_based_launcher.long_delay_ms =
         ini.get<cfg::session_probe::smart_launcher_long_delay>();
    spp.clipboard_based_launcher.short_delay_ms =
        ini.get<cfg::session_probe::smart_launcher_short_delay>();
    spp.clipboard_based_launcher.reset_keyboard_status =
        ini.get<cfg::session_probe::clipboard_based_launcher_reset_keyboard_status>();
    spp.clipboard_based_launcher.ensure_launch_sequence_only_starts_after_logon =
        spp.ensure_launch_sequence_only_starts_after_logon;
    spp.vc_params.end_of_session_check_delay_time = ini.get<cfg::session_probe::end_of_session_check_delay_time>();
    spp.vc_params.ignore_ui_less_processes_during_end_of_session_check =
        ini.get<cfg::session_probe::ignore_ui_less_processes_during_end_of_session_check>();
    spp.vc_params.update_disabled_features                             = ini.get<cfg::session_probe::update_disabled_features>();
    spp.vc_params.childless_window_as_unidentified_input_field =
        ini.get<cfg::session_probe::childless_window_as_unidentified_input_field>();
    spp.is_public_session = ini.get<cfg::session_probe::public_session>();
    spp.vc_params.session_shadowing_support = ini.get<cfg::mod_rdp::session_shadowing_support>();
    spp.vc_params.on_account_manipulation =
        ini.get<cfg::session_probe::on_account_manipulation>();
    spp.vc_params.extra_system_processes =
        ExtraSystemProcesses(ini.get<cfg::session_probe::extra_system_processes>());
    spp.vc_params.outbound_connection_monitor_rules =
        OutboundConnectionMonitorRules(
            ini.get<cfg::session_probe::outbound_connection_monitoring_rules>());
    spp.vc_params.process_monitor_rules =
        ProcessMonitorRules(ini.get<cfg::session_probe::process_monitoring_rules>());
    spp.vc_params.windows_of_these_applications_as_unidentified_input_field = ExtraSystemProcesses(
        ini.get<cfg::session_probe::windows_of_these_applications_as_unidentified_input_field>());
    spp.vc_params.enable_log_rotation = ini.get<cfg::session_probe::enable_log_rotation>();
    spp.vc_params.log_level = ini.get<cfg::session_probe::enable_log>()
        ? ini.get<cfg::session_probe::log_level>()
        : SessionProbeLogLevel::Off;
    spp.vc_params.allow_multiple_handshake = ini.get<cfg::session_probe::allow_multiple_handshake>();
    spp.vc_params.enable_crash_dump = ini.get<cfg::session_probe::enable_crash_dump>();
    spp.vc_params.handle_usage_limit = ini.get<cfg::session_probe::handle_usage_limit>();
    spp.vc_params.memory_usage_limit = ini.get<cfg::session_probe::memory_usage_limit>();

    spp.vc_params.cpu_usage_alarm_threshold = ini.get<cfg::session_probe::cpu_usage_alarm_threshold>();
    spp.vc_params.cpu_usage_alarm_action    = ini.get<cfg::session_probe::cpu_usage_alarm_action>();

    spp.vc_params.disabled_features = ini.get<cfg::session_probe::disabled_features>();
    spp.vc_params.bestsafe_integration = ini.get<cfg::session_probe::enable_bestsafe_interaction>();
    spp.used_to_launch_remote_program = ini.get<cfg::mod_rdp::use_session_probe_to_launch_remote_program>();

    spp.vc_params.at_end_of_session_freeze_connection_and_wait =
        ini.get<cfg::session_probe::at_end_of_session_freeze_connection_and_wait>();

    spp.vc_params.process_command_line_retrieve_method =
        ini.get<cfg::session_probe::process_command_line_retrieve_method>();

    spp.vc_params.periodic_task_run_interval =
        ini.get<cfg::session_probe::periodic_task_run_interval>();
    spp.vc_params.pause_if_session_is_disconnected =
        ini.get<cfg::session_probe::pause_if_session_is_disconnected>();
    spp.vc_params.monitor_own_resources_consumption =
        ini.get<cfg::session_probe::monitor_own_resources_consumption>();

    return spp;
}

inline static ApplicationParams get_rdp_application_params(Inifile & ini)
{
    ApplicationParams ap;
    ap.alternate_shell = ini.get<cfg::mod_rdp::alternate_shell>();
    ap.shell_arguments = ini.get<cfg::mod_rdp::shell_arguments>();

    zstring_view shell_working_directory = ini.get<cfg::mod_rdp::shell_working_directory>();
    const char* sep = strchr(shell_working_directory.c_str(), '*');
    if (sep)
    {
        ap.shadow_invite_time = decimal_chars_to_int<time_t>(shell_working_directory).val;
        ap.shell_working_dir = chars_view{sep + 1, shell_working_directory.end()};
    }
    else
    {
        ap.shell_working_dir = shell_working_directory;
    }
    ap.use_client_provided_alternate_shell = ini.get<cfg::mod_rdp::use_client_provided_alternate_shell>();
    ap.target_application_account = ini.get<cfg::globals::target_application_account>();
    ap.target_application_password = ini.get<cfg::globals::target_application_password>();
    ap.primary_user_id = ini.get<cfg::globals::primary_user_id>();
    ap.target_application = ini.get<cfg::globals::target_application>();

    return ap;
}

} // anonymous namespace

ModPack create_mod_rdp(
    gdi::GraphicApi & drawable,
    gdi::OsdApi & osd,
    RedirectionInfo & redir_info,
    Inifile & ini,
    FrontAPI& front,
    ClientInfo const& client_info_ /* /!\ modified */,
    ClientExecute& rail_client_execute,
    kbdtypes::KeyLocks key_locks,
    Font const& glyphs,
    Theme & theme,
    EventContainer& events,
    SessionLogApi& session_log,
    ErrorMessageCtx& err_msg_ctx,
    Translator const& translator,
    LicenseApi & file_system_license_store,
    Random & gen,
    CryptoContext & cctx,
    std::array<uint8_t, 28>& server_auto_reconnect_packet,
    PerformAutomaticReconnection perform_automatic_reconnection,
    TransportWrapperFnView transport_wrapper_fn)
{
    ClientInfo client_info = client_info_;

    switch (ini.get<cfg::mod_rdp::mode_console>()) {
        case RdpModeConsole::force:
            client_info.console_session = true;
            LOG(LOG_INFO, "Session::mode console : force");
            break;
        case RdpModeConsole::forbid:
            client_info.console_session = false;
            LOG(LOG_INFO, "Session::mode console : forbid");
            break;
        case RdpModeConsole::allow:
            break;
    }

    const bool smartcard_passthrough = ini.get<cfg::mod_rdp::force_smartcard_authentication>();
    const auto rdp_verbose = get_rdp_verbose(ini);

    ModRDPParams mod_rdp_params(
        (smartcard_passthrough ? "" : ini.get<cfg::globals::target_user>().c_str())
      , (smartcard_passthrough ? "" : ini.get<cfg::context::target_password>().c_str())
      , ini.get<cfg::context::target_host>().c_str()
      , "0.0.0.0"   // client ip is silenced
      , key_locks
      , glyphs
      , theme
      , server_auto_reconnect_packet
      , std::move(ini.get_mutable_ref<cfg::context::redirection_password_or_cookie>())
      , translator
      , rdp_verbose
    );

    mod_rdp_params.perform_automatic_reconnection = safe_int{perform_automatic_reconnection};

    //mod_rdp_params.enable_tls                          = true;
    mod_rdp_params.tls_params = ModTlsParamsLoader::rdp(ini);

    mod_rdp_params.enable_nla = mod_rdp_params.target_password[0]
                             && ini.get<cfg::mod_rdp::enable_nla>();
    mod_rdp_params.enable_krb                          = ini.get<cfg::mod_rdp::enable_kerberos>();
    mod_rdp_params.allow_nla_ntlm = ini.get<cfg::mod_rdp::allow_nla_ntlm_fallback>();
    mod_rdp_params.allow_tls_only = ini.get<cfg::mod_rdp::allow_tls_only_fallback>();
    mod_rdp_params.allow_rdp_legacy = ini.get<cfg::mod_rdp::allow_rdp_legacy_fallback>();
    mod_rdp_params.enable_fastpath                     = ini.get<cfg::mod_rdp::fast_path>();

    mod_rdp_params.session_probe_params = get_session_probe_params(ini);

    mod_rdp_params.ignore_auth_channel                 = ini.get<cfg::mod_rdp::ignore_auth_channel>();
    mod_rdp_params.auth_channel                        = ::get_effective_auth_channel_name(CHANNELS::ChannelNameId(ini.get<cfg::mod_rdp::auth_channel>()));
    mod_rdp_params.checkout_channel                    = CHANNELS::ChannelNameId(ini.get<cfg::mod_rdp::checkout_channel>());

    mod_rdp_params.application_params = get_rdp_application_params(ini);

    mod_rdp_params.rdp_compression = ini.get<cfg::mod_rdp::rdp_compression>();
    mod_rdp_params.disconnect_on_logon_user_change = ini.get<cfg::mod_rdp::disconnect_on_logon_user_change>();
    mod_rdp_params.open_session_timeout = ini.get<cfg::mod_rdp::open_session_timeout>();
    mod_rdp_params.enable_server_cert_external_validation = ini.get<cfg::server_cert::enable_external_validation>();
    mod_rdp_params.hide_client_name = ini.get<cfg::mod_rdp::hide_client_name>();
    mod_rdp_params.enable_persistent_disk_bitmap_cache = ini.get<cfg::mod_rdp::persistent_disk_bitmap_cache>();
    mod_rdp_params.enable_cache_waiting_list = ini.get<cfg::mod_rdp::cache_waiting_list>();
    mod_rdp_params.persist_bitmap_cache_on_disk = ini.get<cfg::mod_rdp::persist_bitmap_cache_on_disk>();
    mod_rdp_params.password_printing_mode = ini.get<cfg::debug::password>();
    mod_rdp_params.cache_verbose = safe_cast<BmpCache::Verbose>(ini.get<cfg::debug::cache>());

    mod_rdp_params.disabled_orders                     += parse_primary_drawing_orders(
        ini.get<cfg::mod_rdp::disabled_orders>().c_str(),
        bool(rdp_verbose & (RDPVerbose::basic_trace | RDPVerbose::capabilities)));

    mod_rdp_params.bogus_freerdp_clipboard             = ini.get<cfg::mod_rdp::bogus_freerdp_clipboard>();
    mod_rdp_params.bogus_refresh_rect                  = ini.get<cfg::mod_rdp::bogus_refresh_rect>();

    mod_rdp_params.drive_params.proxy_managed_drives   = ini.get<cfg::mod_rdp::proxy_managed_drives>().c_str();
    mod_rdp_params.drive_params.proxy_managed_prefix   = app_path(AppPath::DriveRedirection);

    mod_rdp_params.allow_using_multiple_monitors       = ini.get<cfg::client::allow_using_multiple_monitors>();
    mod_rdp_params.bogus_monitor_layout_treatment      = ini.get<cfg::mod_rdp::bogus_monitor_layout_treatment>();
    mod_rdp_params.allow_scale_factor                  = ini.get<cfg::client::allow_scale_factor>();

    mod_rdp_params.adjust_performance_flags_for_recording
            = (ini.get<cfg::globals::is_rec>()
            && ini.get<cfg::mod_rdp::auto_adjust_performance_flags>()
            && ((ini.get<cfg::capture::capture_flags>()
                & (CaptureFlags::wrm | CaptureFlags::ocr)) != CaptureFlags::none));

    auto & rap = mod_rdp_params.remote_app_params;
    {
        rap.rail_client_execute = &rail_client_execute;
        rap.windows_execute_shell_params = rail_client_execute.get_windows_execute_shell_params();

        bool const rail_is_required = (ini.get<cfg::mod_rdp::use_native_remoteapp_capability>()
            && (!mod_rdp_params.application_params.target_application.empty()
             || (ini.get<cfg::mod_rdp::use_client_provided_remoteapp>()
                && not rap.windows_execute_shell_params.exe_or_file.empty())));

        bool wabam_uses_translated_remoteapp
            = ini.get<cfg::mod_rdp::wabam_uses_translated_remoteapp>()
            && ini.get<cfg::context::is_wabam>();

        rap.should_ignore_first_client_execute
            = rail_client_execute.should_ignore_first_client_execute();
        rap.enable_remote_program = ((client_info.remote_program || wabam_uses_translated_remoteapp ||
                                      ini.get<cfg::mod_rdp::all_clients_use_translated_remoteapp>())
            && rail_is_required);
        rap.remote_program_enhanced = client_info.remote_program_enhanced;
        rap.convert_remoteapp_to_desktop = (!client_info.remote_program
            && (wabam_uses_translated_remoteapp ||
                ini.get<cfg::mod_rdp::all_clients_use_translated_remoteapp>())
            && rail_is_required);
        rap.use_client_provided_remoteapp = ini.get<cfg::mod_rdp::use_client_provided_remoteapp>();
        rap.rail_disconnect_message_delay = ini.get<cfg::mod_rdp::remote_programs_disconnect_message_delay>();
        rap.bypass_legal_notice_delay = ini.get<cfg::mod_rdp::remoteapp_bypass_legal_notice_delay>();
        rap.bypass_legal_notice_timeout = ini.get<cfg::mod_rdp::remoteapp_bypass_legal_notice_timeout>();
    }

    mod_rdp_params.replace_null_pointer_by_default_pointer = ini.get<cfg::mod_rdp::replace_null_pointer_by_default_pointer>();
    mod_rdp_params.large_pointer_support               = ini.get<cfg::globals::large_pointer_support>();
    mod_rdp_params.load_balance_info                   = ini.get<cfg::mod_rdp::load_balance_info>().c_str();

    // ======================= File System Params ===================
    {
        auto & fsp = mod_rdp_params.file_system_params;
        fsp.bogus_ios_rdpdr_virtual_channel     = ini.get<cfg::mod_rdp::bogus_ios_rdpdr_virtual_channel>();
        fsp.enable_rdpdr_data_analysis          =  ini.get<cfg::mod_rdp::enable_rdpdr_data_analysis>();
    }
    // ======================= End File System Params ===================


    // ======================= Dynamic Channel Params ===================

    mod_rdp_params.dynamic_channels_params.allowed_channels = ini.get<cfg::mod_rdp::allowed_dynamic_channels>();
    mod_rdp_params.dynamic_channels_params.denied_channels  = ini.get<cfg::mod_rdp::denied_dynamic_channels>();

    // ======================= End Dynamic Channel Params ===================

    mod_rdp_params.split_domain = ini.get<cfg::mod_rdp::split_domain>();

    mod_rdp_params.enable_remotefx = ini.get<cfg::mod_rdp::enable_remotefx>();
    mod_rdp_params.use_license_store = ini.get<cfg::mod_rdp::use_license_store>();

    mod_rdp_params.allow_session_reconnection_by_shortcut
        = ini.get<cfg::mod_rdp::allow_session_reconnection_by_shortcut>();

    mod_rdp_params.windows_xp_clipboard_support
        = ini.get<cfg::mod_rdp::windows_xp_clipboard_support>();

    mod_rdp_params.block_user_input_until_appdriver_completes
        = ini.get<cfg::mod_rdp::block_user_input_until_appdriver_completes>();

    mod_rdp_params.enable_restricted_admin_mode = ini.get<cfg::mod_rdp::enable_restricted_admin_mode>();
    mod_rdp_params.file_system_params.smartcard_passthrough        = smartcard_passthrough;
    mod_rdp_params.forward_client_build_number = ini.get<cfg::mod_rdp::forward_client_build_number>();

    mod_rdp_params.save_session_info_pdu = ini.get<cfg::protocol::save_session_info_pdu>();

    mod_rdp_params.session_probe_params.alternate_directory_environment_variable = ini.get<cfg::session_probe::alternate_directory_environment_variable>();
    size_t const SESSION_PROBE_ALTERNATE_DIRECTORY_ENVIRONMENT_VARIABLE_NAME_MAX_LENGTH = 3;
    size_t const alternate_directory_environment_variable_length = mod_rdp_params.session_probe_params.alternate_directory_environment_variable.length();
    if (alternate_directory_environment_variable_length) {
        if (alternate_directory_environment_variable_length > SESSION_PROBE_ALTERNATE_DIRECTORY_ENVIRONMENT_VARIABLE_NAME_MAX_LENGTH) {
            mod_rdp_params.session_probe_params.alternate_directory_environment_variable.resize(SESSION_PROBE_ALTERNATE_DIRECTORY_ENVIRONMENT_VARIABLE_NAME_MAX_LENGTH);
        }

        mod_rdp_params.session_probe_params.customize_executable_name = true;
    }

    if (mod_rdp_params.session_probe_params.customize_executable_name) {
        mod_rdp_params.session_probe_params.vc_params.enable_self_cleaner = ini.get<cfg::session_probe::enable_cleaner>();
    }

    mod_rdp_params.session_probe_params.vc_params.enable_remote_program =
        mod_rdp_params.remote_app_params.enable_remote_program;

    Rect const adjusted_client_execute_rect = rail_client_execute.adjust_rect(client_info.get_widget_rect());

    const bool host_mod_in_widget = (client_info.remote_program
        && !mod_rdp_params.remote_app_params.enable_remote_program);

    if (host_mod_in_widget) {
        client_info.screen_info.width  = adjusted_client_execute_rect.cx / 4 * 4;
        client_info.screen_info.height = adjusted_client_execute_rect.cy;
        client_info.cs_monitor = GCC::UserData::CSMonitor{};
    }
    else {
        rail_client_execute.reset(false);
    }

    if (auto const& resolution = ini.get<cfg::mod_rdp::force_screen_resolution>()
      ; resolution.is_valid()
        && (!rap.enable_remote_program || rap.convert_remoteapp_to_desktop)
    ) {
        client_info.screen_info.width  = resolution.width;
        client_info.screen_info.height = resolution.height;
        // prevent the change of resolution during the session
        mod_rdp_params.dynamic_channels_params.denied_channels += ",Microsoft::Windows::RDS::DisplayControl";
    }

    if (ini.get<cfg::mod_rdp::disable_coreinput_dynamic_channel>())
    {
        mod_rdp_params.dynamic_channels_params.denied_channels += ",Microsoft::Windows::RDS::CoreInput";
    }

    // ================== Application Driver =========================
    update_application_driver(mod_rdp_params, ini);
    // ================== End Application Driver ======================

    // account used for Kerberos ticket armoring
    mod_rdp_params.krb_armoring_user = ini.get<cfg::mod_rdp::effective_krb_armoring_user>().c_str();
    mod_rdp_params.krb_armoring_password = ini.get<cfg::mod_rdp::effective_krb_armoring_password>().c_str();

    auto connect_to_rdp_target_host = [&ini, &session_log, &err_msg_ctx](
        bool enable_ipv6,
        std::chrono::milliseconds connection_establishment_timeout,
        std::chrono::milliseconds tcp_user_timeout,
        time_t shadow_invite_time
    ) {
        try
        {
            return connect_to_target_host(
                ini, session_log, err_msg_ctx,
                trkeys::authentification_rdp_fail, enable_ipv6,
                connection_establishment_timeout,
                tcp_user_timeout);
        }
        catch(const Error& e)
        {
            if (e.id == ERR_SOCKET_CONNECT_FAILED && shadow_invite_time)
            {
                if (time(nullptr) - shadow_invite_time > 30) {
                    err_msg_ctx.set_msg(trkeys::target_shadow_fail);
                }
            }

            throw;
        }
    };

    unique_fd client_sck = ini.get<cfg::context::tunneling_target_host>().empty()
        ? connect_to_rdp_target_host(
            ini.get<cfg::mod_rdp::enable_ipv6>(),
            ini.get<cfg::all_target_mod::connection_establishment_timeout>(),
            ini.get<cfg::all_target_mod::tcp_user_timeout>(),
            mod_rdp_params.application_params.shadow_invite_time)
        : addr_connect(
            ini.get<cfg::context::tunneling_target_host>().c_str(),
            std::chrono::seconds(1), false);
    mod_rdp_params.session_probe_params.vc_params.target_ip = ini.get<cfg::context::ip_target>();

    IpAddress local_ip_address;
    switch (ini.get<cfg::mod_rdp::client_address_sent>())
    {
        case ClientAddressSent::no_address :
            break;
        case ClientAddressSent::front :
            mod_rdp_params.client_address = ini.get<cfg::globals::host>().c_str();
            break;
        case ClientAddressSent::proxy :
            if (!get_local_ip_address(local_ip_address, client_sck.fd())) {
                throw Error(ERR_SOCKET_CONNECT_FAILED);
            }
            mod_rdp_params.client_address = local_ip_address.ip_addr;
            break;
    }

    std::unique_ptr<RailModuleHostMod> host_mod {
        host_mod_in_widget
        ? create_mod_rail(ini,
                          events,
                          drawable,
                          client_info,
                          rail_client_execute,
                          glyphs,
                          theme)
        : nullptr
    };

    ChannelsAuthorizations channels_authorizations = make_channels_authorizations(ini);

    if (ini.get<cfg::context::is_wabam>()
     && ini.get<cfg::session_probe::smart_launcher_enable_wabam_affinity>()
     && channels_authorizations.is_authorized(CHANNELS::channel_names::cliprdr)
     && mod_rdp_params.session_probe_params.enable_session_probe
     ) {
        LOG(LOG_INFO, "Session Probe Clipboard Based Launche enables AM Affinity");
        mod_rdp_params.session_probe_params.clipboard_based_launcher.clipboard_initialization_delay_ms =
            std::chrono::milliseconds(120000);
    }

    auto new_mod = std::make_unique<ModRDPWithSocket>(
        osd,
        ini,
        "RDP Target"_sck_name,
        std::move(client_sck),
        safe_cast<SocketTransport::Verbose>(ini.get<cfg::debug::sck_mod>()),
        events,
        session_log,
        err_msg_ctx,
        host_mod ? host_mod->proxy_gd() : drawable,
        front,
        client_info,
        redir_info,
        gen,
        cctx,
        channels_authorizations,
        mod_rdp_params,
        file_system_license_store,
        ini,
        ini.get<cfg::debug::mod_rdp_use_failure_simulation_socket_transport>(),
        transport_wrapper_fn
    );

    auto* socket_transport = &new_mod->get_transport();

    if (!host_mod) {
        auto mod = new_mod.release();
        return ModPack{not_null_ptr{mod}, mod->get_windowing_api(), socket_transport, true};
    }

    host_mod->set_mod(std::move(new_mod));
    auto mod = host_mod.release();
    return ModPack{not_null_ptr{mod}, &rail_client_execute, socket_transport, true};
}
