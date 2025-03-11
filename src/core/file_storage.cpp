/*
SPDX-FileCopyrightText: 2026 Wallix Proxies Team
SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "acl/auth_api.hpp"
#include "capture/fdx_capture.hpp"
#include "configs/config.hpp"
#include "core/file_storage.hpp"
#include "utils/strutils.hpp"


FileStorage::FileStorage() noexcept = default;
FileStorage::~FileStorage() = default;

FileStorageParams FileStorage::init(
    Random & gen,
    Inifile & ini,
    CryptoContext & cctx,
    SessionLogApi & session_log,
    Params params)
{
    assert(!m_fdx_capture);

    /*
     * Check if storage is enabled
     */

    auto store_file = ini.get<cfg::file_storage::store_file>();

    switch (store_file)
    {
        case RdpStoreFile::never:
            return {};
        case RdpStoreFile::on_invalid_verification:
            if (!(params.up_file_authorized && ini.get<cfg::file_verification::enable_up>())
             && !(params.down_file_authorized && ini.get<cfg::file_verification::enable_down>()))
            {
                return {};
            }
            [[fallthrough]];
        case RdpStoreFile::always:
            if (!params.up_file_authorized && !params.down_file_authorized)
            {
                return {};
            }
    }

    /*
     * Build storage
     */

    LOG(LOG_INFO, "Enable clipboard file storage");
    auto const& session_id = ini.get<cfg::context::session_id>();
    auto const& subdir = ini.get<cfg::capture::record_subdirectory>();
    auto const& record_dir = ini.get<cfg::capture::record_path>();
    auto const& hash_dir = ini.get<cfg::capture::hash_path>();
    auto const& filebase = ini.get<cfg::capture::record_filebase>();

    m_fdx_capture = std::make_unique<FdxCapture>(
        str_concat(record_dir.as_string(), subdir),
        str_concat(hash_dir.as_string(), subdir),
        filebase,
        session_id, ini.get<cfg::capture::file_permissions>(),
        cctx, gen,
        [&session_log](const Error & error){
            if (error.errnum == ENOSPC) {
                session_log.acl_report(AclReport::file_system_full());
            }
        });

    ini.set_acl<cfg::capture::fdx_path>(m_fdx_capture->get_fdx_path());

    return {
        .fdx_capture = m_fdx_capture.get(),
        .always_file_storage = (store_file == RdpStoreFile::always),
    };
}
