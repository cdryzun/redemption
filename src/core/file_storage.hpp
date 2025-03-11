/*
SPDX-FileCopyrightText: 2026 Wallix Proxies Team
SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "core/events.hpp"
#include "core/file_storage_params.hpp"

#include <memory>


class FdxCapture;
class SessionLogApi;
class Inifile;
class Random;
class CryptoContext;


struct FileStorage
{
    FileStorage() noexcept;
    ~FileStorage();

    struct Params
    {
        bool up_file_authorized;
        bool down_file_authorized;
    };

    [[nodiscard]]
    FileStorageParams init(
        Random & gen,
        Inifile & ini,
        CryptoContext & cctx,
        SessionLogApi & session_log,
        Params params
    );

private:
    std::unique_ptr<FdxCapture> m_fdx_capture;
};
