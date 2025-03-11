/*
SPDX-FileCopyrightText: 2026 Wallix Proxies Team
SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "acl/auth_api.hpp"
#include "configs/config.hpp"
#include "core/log_id.hpp"
#include "core/file_validator/file_validator.hpp"
#include "core/file_validator/file_validator_service.hpp"
#include "transport/file_transport.hpp"
#include "utils/netutils.hpp"


static void file_verification_error(
    SessionLogApi& session_log,
    FileValidatorTargets targets,
    chars_view msg)
{
    auto log = [&](FileValidatorTargets mask) {
        if (flags_any(targets, mask)) {
            session_log.log6(LogId::FILE_VERIFICATION_ERROR, KVLogList{
                KVLog("icap_service"_av, file_validator_target_to_string(mask)),
                KVLog("status"_av, msg),
            });
        }
    };

    log(FileValidatorTargets::Upload);
    log(FileValidatorTargets::Download);
}


struct FileValidator::FileValidatorCtx
{
    struct CtxError
    {
        SessionLogApi & session_log;
        FileValidatorTargets targets;
    };

private:
    struct FileValidatorTransport final : FileTransport
    {
        using FileTransport::FileTransport;

        size_t do_partial_read(uint8_t * buffer, size_t len) override
        {
            size_t r = FileTransport::do_partial_read(buffer, len);
            if (r == 0) {
                LOG(LOG_ERR, "FileValidator::do_partial_read: No data read!");
                this->throw_error(Error(ERR_TRANSPORT_NO_MORE_DATA, errno));
            }
            return r;
        }
    };

    CtxError ctx_error;
    FileValidatorTransport trans;

public:
    // TODO wait result (add delay)
    FileValidatorService service;

    FileValidatorCtx(unique_fd&& fd, CtxError&& ctx_error, FileValidatorService::Verbose verbose)
        : ctx_error(std::move(ctx_error))
        , trans(std::move(fd), [this](const Error & err){
            file_verification_error(
                this->ctx_error.session_log,
                this->ctx_error.targets,
                err.errmsg()
            );
        })
        , service(this->trans, verbose)
    {}

    ~FileValidatorCtx()
    {
        try {
            this->service.send_close_session();
        }
        catch (...) {
        }
    }

    int get_fd() const
    {
        return this->trans.get_fd();
    }
};


FileValidator::FileValidator() noexcept = default;
FileValidator::~FileValidator() = default;

FileValidator::ResultWithFd FileValidator::init_impl(
    Inifile & ini,
    SessionLogApi & session_log,
    Params params)
{
    assert(!m_file_validator);

    /*
     * Check if validator is enabled
     */

    bool validator_up
        = (params.up.file_authorized
           || (params.up.text_authorized && ini.get<cfg::file_verification::clipboard_text_up>()))
       && ini.get<cfg::file_verification::enable_up>();

    bool validator_down
        = (params.down.file_authorized
           || (params.down.text_authorized && ini.get<cfg::file_verification::clipboard_text_down>()))
       && ini.get<cfg::file_verification::enable_down>();

    if (!validator_up && !validator_down)
    {
        return {};
    }

    /*
     * Build validator
     */

    auto validator_targets
        = (validator_up ? FileValidatorTargets::Upload : FileValidatorTargets::None)
        | (validator_down ? FileValidatorTargets::Download : FileValidatorTargets::None)
        ;

    auto const & socket_path = ini.get<cfg::file_verification::socket_path>();
    bool const no_log_for_unix_socket = false;
    unique_fd ufd = addr_connect_blocking(
        socket_path.c_str(),
        ini.get<cfg::all_target_mod::connection_establishment_timeout>(),
        no_log_for_unix_socket
    );

    if (!ufd) {
        LOG(LOG_ERR, "Error, can't connect to validator, file validation disable");
        file_verification_error(
            session_log,
            validator_targets,
            "Unable to connect to FileValidator service"_av
        );
        throw Error(ERR_SOCKET_CONNECT_FAILED);
    }

    m_file_validator = std::make_unique<FileValidatorCtx>(
        std::move(ufd),
        FileValidatorCtx::CtxError {
            session_log,
            validator_targets,
        },
        params.verbose_validator
            ? FileValidatorService::Verbose::Yes
            : FileValidatorService::Verbose::No
    );

    m_file_validator->service.send_infos({
        "server_ip"_av, ini.get<cfg::context::target_host>(),
        "client_ip"_av, ini.get<cfg::globals::host>(),
        "auth_user"_av, ini.get<cfg::globals::auth_user>()
    });

    return {
        m_file_validator->get_fd(),
        {
            .file_validator_service = &m_file_validator->service,
            .targets = validator_targets,
            .log_if_accepted = ini.get<cfg::file_verification::log_if_accepted>(),
            .block_invalid_file_upload
                = ini.get<cfg::file_verification::block_invalid_file_up>(),
            .block_invalid_file_download
                = ini.get<cfg::file_verification::block_invalid_file_down>(),
            .max_blocked_file_size_rejected
                = safe_cast<uint64_t>(ini.get<cfg::file_verification::max_file_size_rejected>())
                * 1024u * 1024u /* mebibyte to byte */,
            .tmp_dir = ini.get<cfg::file_verification::tmpdir>().as_string(),
        },
    };
}
