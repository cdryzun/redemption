/*
SPDX-FileCopyrightText: 2026 Wallix Proxies Team
SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "configs/config.hpp"
#include "core/client_info.hpp"
#include "core/file_storage.hpp"
#include "core/file_validator/file_validator.hpp"
#include "mod/vnc/vnc.hpp"
#include "mod/internal/rail_module_host_mod.hpp"
#include "mod/tls_params_loader.hpp"
#include "transport/socket_transport.hpp"
#include "acl/module_manager/create_module_vnc.hpp"
#include "acl/module_manager/create_module_rail.hpp"
#include "acl/connect_to_target_host.hpp"
#include "utils/sugar/unique_fd.hpp"
#include "utils/sugar/split.hpp"
#include "utils/netutils.hpp"
#include "RAIL/client_execute.hpp"


namespace
{

VNCVerbose get_vnc_verbose(Inifile const & ini) noexcept
{
    return safe_cast<VNCVerbose>(ini.get<cfg::debug::mod_vnc>());
}

struct VncData
{
    SocketTransport & get_transport()
    {
        return this->socket_transport;
    }

    EventsGuard events_guard;
    FileStorage file_storage_ctx {};
    FileValidator file_validator_ctx {};
    Random & gen;
    Inifile & ini;
    CryptoContext & cctx;
    SessionLogApi & session_log;
    bool enable_file_upload = false;
    bool enable_file_download = false;
    SocketTransport socket_transport;
};

struct ModVNCWithSocket final : VncData, mod_vnc
{
    ModVNCWithSocket(
        EventContainer & events,
        Inifile & ini,
        unique_fd sck,
        gdi::GraphicApi & drawable,
        FrontAPI& front,
        ClientInfo const& client_info,
        ClientExecute& rail_client_execute,
        KeyLayout const& layout,
        kbdtypes::KeyLocks locks,
        Font const& glyphs,
        SessionLogApi& session_log,
        Translator const& translator,
        Random & gen,
        CryptoContext & cctx
    )
    : VncData {
        .events_guard { events },
        .gen = gen,
        .ini = ini,
        .cctx = cctx,
        .session_log = session_log,
        .socket_transport {
            "VNC Target"_sck_name, std::move(sck),
            ini.get<cfg::context::target_host>(),
            checked_int(ini.get<cfg::context::target_port>()),
            ini.get<cfg::all_target_mod::connection_establishment_timeout>(),
            ini.get<cfg::all_target_mod::tcp_user_timeout>(),
            std::chrono::milliseconds(ini.get<cfg::globals::mod_recv_timeout>()),
            safe_cast<SocketTransport::Verbose>(ini.get<cfg::debug::sck_mod>())
        }
    }
    , mod_vnc(
        get_transport(),
        gen,
        drawable,
        glyphs,
        events,
        ini.get<cfg::globals::target_user>().c_str(),
        ini.get<cfg::context::target_password>().c_str(),
        front,
        client_info.screen_info.width,
        client_info.screen_info.height,
        [&]{
            auto use_proxy_opt = ini.get<cfg::globals::enable_wab_integration>();

            ModVncParams vnc_params {
                .cut_text = {
                    .enable_upload = !use_proxy_opt
                                  && ini.get<cfg::vnc_clipboard::enable_clipboard_upload>(),
                    .enable_download = !use_proxy_opt
                                    && ini.get<cfg::vnc_clipboard::enable_clipboard_download>(),
                    .server_encoding = ini.get<cfg::vnc_clipboard::clipboard_encoding>(),
                    .bogus_infinite_loop_strategy
                        = ini.get<cfg::vnc_clipboard::bogus_infinite_loop_strategy>(),
                },
                .file_transfer = {
                    .enable_upload = !use_proxy_opt
                                  && ini.get<cfg::vnc_file_transfer::enable_file_upload>(),
                    .enable_download = !use_proxy_opt
                                    && ini.get<cfg::vnc_file_transfer::enable_file_download>(),
                    .max_item_in_gui = ini.get<cfg::vnc_file_transfer::max_item_in_gui>(),
                    .max_file_list = ini.get<cfg::vnc_file_transfer::max_file_transfer_list>(),
                    .max_file_size = ini.get<cfg::vnc_file_transfer::max_file_size>(),
                },
                .get_file_validator_and_storage = [this] {
                    return this->make_file_validator_and_storage();
                },
            };

            if (use_proxy_opt) {
                for (auto opt : split_with(ini.get<cfg::context::proxy_opt>(), ',')) {
                    auto opt_name = opt.as<std::string_view>();
                    if (opt_name == "VNC_CLIPBOARD_UP") vnc_params.cut_text.enable_upload = true;
                    if (opt_name == "VNC_CLIPBOARD_DOWN") vnc_params.cut_text.enable_download = true;
                    if (opt_name == "VNC_FILE_UP") vnc_params.file_transfer.enable_upload = true;
                    if (opt_name == "VNC_FILE_DOWN") vnc_params.file_transfer.enable_download = true;
                }
            }

            this->enable_file_upload = vnc_params.file_transfer.enable_upload;
            this->enable_file_download = vnc_params.file_transfer.enable_download;

            return vnc_params;
        }(),
        ini.get<cfg::mod_vnc::encodings>().c_str(),
        layout,
        locks,
        ini.get<cfg::mod_vnc::server_is_macos>(),
        ini.get<cfg::mod_vnc::server_unix_alt>(),
        ini.get<cfg::mod_vnc::support_cursor_pseudo_encoding>(),
        (client_info.remote_program ? &rail_client_execute : nullptr),
        get_vnc_verbose(ini),
        session_log,
        ModTlsParamsLoader::vnc(ini),
        ini.get<cfg::mod_vnc::force_authentication_method>(),
        translator
    )
    {}

    ModVncParams::FileValidatorAndStorage make_file_validator_and_storage()
    {
        auto & d = static_cast<VncData&>(*this);
        auto & ini = d.ini;

        FileValidator::Params validators_params {
            .up = {
                .text_authorized = false,
                .file_authorized = enable_file_upload,
            },
            .down = {
                .text_authorized = false,
                .file_authorized = enable_file_download,
            },
            .verbose_validator = bool(get_vnc_verbose(ini) & VNCVerbose::clipboard),
        };

        return ModVncParams::FileValidatorAndStorage {
            .file_validator = file_validator_ctx.init(
                ini,
                d.session_log,
                validators_params,
                d.events_guard,
                [this](Event& /*event*/)
                {
                    this->file_validator_receive_event();
                }
            ),
            .file_storage = file_storage_ctx.init(
                d.gen, d.ini, d.cctx, d.session_log,
                {
                    .up_file_authorized = d.enable_file_upload,
                    .down_file_authorized = d.enable_file_download,
                }
            ),
        };
    }
};

} // anonymous namespace

ModPack create_mod_vnc(
    gdi::GraphicApi & drawable,
    Inifile& ini, FrontAPI& front, ClientInfo const& client_info,
    ClientExecute& rail_client_execute,
    KeyLayout const& layout,
    kbdtypes::KeyLocks locks,
    Font const& glyphs,
    Theme & theme,
    EventContainer& events,
    SessionLogApi& session_log,
    ErrorMessageCtx& err_msg_ctx,
    Translator const& translator,
    Random & rand,
    CryptoContext & cctx
)
{
    LOG(LOG_INFO, "ModuleManager::Creation of new mod 'VNC'");

    unique_fd client_sck = ini.get<cfg::context::tunneling_target_host>().empty()
        ? connect_to_target_host(
            ini, session_log, err_msg_ctx, trkeys::authentification_vnc_fail,
            ini.get<cfg::mod_vnc::enable_ipv6>(),
            ini.get<cfg::all_target_mod::connection_establishment_timeout>(),
            ini.get<cfg::all_target_mod::tcp_user_timeout>())
        : addr_connect(
            ini.get<cfg::context::tunneling_target_host>().c_str(),
            std::chrono::seconds(1), false);

    std::unique_ptr<RailModuleHostMod> host_mod {
        client_info.remote_program
        ? create_mod_rail(ini,
                          events,
                          drawable,
                          client_info,
                          rail_client_execute,
                          glyphs,
                          theme)
        : nullptr
    };

    auto new_mod = std::make_unique<ModVNCWithSocket>(
        events,
        ini,
        std::move(client_sck),
        drawable,
        front,
        client_info,
        rail_client_execute,
        layout,
        locks,
        glyphs,
        session_log,
        translator,
        rand,
        cctx
    );

    auto* socket_transport = &new_mod->get_transport();

    if (!client_info.remote_program) {
        return ModPack{not_null_ptr{new_mod.release()}, nullptr, socket_transport, false};
    }

    host_mod->set_mod(std::move(new_mod));
    return ModPack{not_null_ptr{host_mod.release()}, nullptr, socket_transport, false};
}
