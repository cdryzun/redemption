/*
SPDX-FileCopyrightText: 2026 Wallix Proxies Team
SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "core/file_validator/file_validator_params.hpp"
#include "core/events.hpp"

#include <memory>


class FileValidatorService;
class SessionLogApi;
class Inifile;


struct FileValidator
{
    FileValidator() noexcept;
    ~FileValidator();

    struct Params
    {
        struct Authorizations
        {
            bool text_authorized;
            bool file_authorized;

            bool is_disabled() const noexcept
            {
                return !(text_authorized || file_authorized);
            }
        };
        Authorizations up;
        Authorizations down;
        bool verbose_validator;
    };

    template<class FnEvent>
    [[nodiscard]]
    FileValidatorParams init(
        Inifile & ini,
        SessionLogApi & session_log,
        Params params,
        EventsGuard & events_guard,
        FnEvent fn_event
    )
    {
        auto validator_and_fd = init_impl(ini, session_log, params);
        if (validator_and_fd.params.file_validator_service)
        {
            events_guard.create_event_fd_without_timeout(
                "File Validator Event",
                validator_and_fd.fd,
                fn_event
            );
        }
        return validator_and_fd.params;
    }

private:
    struct FileValidatorCtx;

    struct ResultWithFd
    {
        int fd;
        FileValidatorParams params;
    };

    ResultWithFd init_impl(
        Inifile & ini,
        SessionLogApi & session_log,
        Params params
    );

    std::unique_ptr<FileValidatorCtx> m_file_validator;
};
