/*
    This program is free software; you can redistribute it and/or modify it
     under the terms of the GNU General Public License as published by the
     Free Software Foundation; either version 2 of the License, or (at your
     option) any later version.

    This program is distributed in the hope that it will be useful, but
     WITHOUT ANY WARRANTY; without even the implied warranty of
     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
     Public License for more details.

    You should have received a copy of the GNU General Public License along
     with this program; if not, write to the Free Software Foundation, Inc.,
     675 Mass Ave, Cambridge, MA 02139, USA.

    Product name: redemption, a FLOSS RDP proxy
    Copyright (C) Wallix 2017
    Author(s): Christophe Grosjean, Raphael Zhou
*/

#pragma once

#include "acl/license_api.hpp"
#include "utils/fileutils.hpp"
#include "utils/log.hpp"
#include "utils/sugar/unique_fd.hpp"
#include "utils/sugar/byte_copy.hpp"

#include <algorithm>
#include <string>
#include <cstring>

#include <sys/stat.h>

/**
 * License file name format:
 *  license_index_v0 = 0x{version_in_8_hex_digits}_{scope}_{company_name}_{product_id}
 *  license_index_v1 = 0.0.0.0_0x{version_in_8_hex_digits}_{scope}_{company_name}_{product_id}
 *  license_index = license_index_v0 | license_index_v1
 *  license_index = license_index.replace(' ', '-')
 *  {license_path}/{client_name}/{license_index}
 */
class FileSystemLicenseStore : public LicenseApi
{
public:
    FileSystemLicenseStore(std::string license_path_)
    : license_path(std::move(license_path_))
    {}

    // The functions shall return empty bytes_view to indicate the error.
    bytes_view get_license_v1(
        char const* client_name, uint32_t version, char const* scope,
        char const* company_name, char const* product_id,
        writable_sized_bytes_view<LIC::LICENSE_HWID_SIZE> hwid,
        writable_bytes_view out, bool enable_log) override
    {
        auto truncated_error = [&]{
            LOG(LOG_ERR, "FileSystemLicenseStore::get_license_v1: temporary filename for Licence truncated: %s/%s/0.0.0.0_0x%08X_%s_%s_%s",
                license_path.c_str(), client_name, version, scope, company_name, product_id);
            return bytes_view { out.data(), 0 };
        };

        PathMaker path_maker;
        if (!path_maker.push_dir(license_path, chars_view{client_name, strlen(client_name)})
         || !path_maker.push_filename(enable_log, "get_license_v1", "0.0.0.0_", version, scope, company_name, product_id)
        ) {
            return truncated_error();
        }

        if (unique_fd ufd{::open(path_maker.path, O_RDONLY)}) {
            ssize_t number_of_bytes_read = ::read(ufd.fd(), hwid.data(), hwid.size());
            if (number_of_bytes_read != hwid.size()) {
                LOG(LOG_ERR, "FileSystemLicenseStore::get_license_v1: license file truncated (1) : expected %zu, got %zu", hwid.size(), number_of_bytes_read);
            }
            else {
                uint32_t license_size = 0;
                number_of_bytes_read = ::read(ufd.fd(), &license_size, sizeof(license_size));
                if (number_of_bytes_read != sizeof(license_size)) {
                    LOG(LOG_ERR, "FileSystemLicenseStore::get_license_v1: license file truncated (2) : expected %zu, got %zu", sizeof(license_size), number_of_bytes_read);
                }
                else {
                    if (out.size() >= license_size)
                    {
                        number_of_bytes_read = ::read(ufd.fd(), out.data(), license_size);
                        if (number_of_bytes_read != license_size) {
                            LOG(LOG_ERR, "FileSystemLicenseStore::get_license_v1: license file truncated (3) : expected %u, got %zu", license_size, number_of_bytes_read);
                        }
                        else {
                            LOG(LOG_INFO, "FileSystemLicenseStore::get_license_v1: LicenseSize=%u", license_size);

                            return bytes_view { out.data(), license_size };
                        }
                    }
                }
            }
        }
        else {
            LOG(LOG_WARNING, "FileSystemLicenseStore::get_license_v1: Failed to open license file! Path=\"%s\" errno=%s(%d)", path_maker.path, strerror(errno), errno);
        }

        return bytes_view { out.data(), 0 };
    }

    // The functions shall return empty bytes_view to indicate the error.
    bytes_view get_license_v0(
        char const* client_name, uint32_t version, char const* scope,
        char const* company_name, char const* product_id,
        writable_bytes_view out, bool enable_log) override
    {
        auto truncated_error = [&]{
            LOG(LOG_ERR, "FileSystemLicenseStore::get_license_v0: temporary filename for Licence truncated: %s/%s/0x%08X_%s_%s_%s",
                license_path.c_str(), client_name, version, scope, company_name, product_id);
            return bytes_view { out.data(), 0 };
        };

        PathMaker path_maker;
        if (!path_maker.push_dir(license_path, chars_view{client_name, strlen(client_name)})
         || !path_maker.push_filename(enable_log, "get_license_v0", "", version, scope, company_name, product_id)
        ) {
            return truncated_error();
        }

        if (unique_fd ufd{::open(path_maker.path, O_RDONLY)}) {
            uint32_t license_size = 0;
            ssize_t number_of_bytes_read = ::read(ufd.fd(), &license_size, sizeof(license_size));
            if (number_of_bytes_read != sizeof(license_size)) {
                LOG(LOG_ERR, "FileSystemLicenseStore::get_license_v0: license file truncated (1) : expected %zu, got %zu", sizeof(license_size), number_of_bytes_read);
            }
            else {
                if (out.size() >= license_size)
                {
                    number_of_bytes_read = ::read(ufd.fd(), out.data(), license_size);
                    if (number_of_bytes_read != license_size) {
                        LOG(LOG_ERR, "FileSystemLicenseStore::get_license_v0: license file truncated (2) : expected %u, got %zu", license_size, number_of_bytes_read);
                    }
                    else {
                        LOG(LOG_INFO, "FileSystemLicenseStore::get_license_v0: LicenseSize=%u", license_size);

                        return bytes_view { out.data(), license_size };
                    }
                }
            }
        }
        else {
            LOG(LOG_WARNING, "FileSystemLicenseStore::get_license_v0: Failed to open license file! Path=\"%s\" errno=%s(%d)", path_maker.path, strerror(errno), errno);
        }

        return bytes_view { out.data(), 0 };
    }

    bool put_license(
        char const* client_name, uint32_t version, char const* scope,
        char const* company_name, char const* product_id,
        sized_bytes_view<LIC::LICENSE_HWID_SIZE> hwid,
        bytes_view in, bool enable_log) override
    {
        auto truncated_error = [&] {
            LOG(LOG_ERR, "FileSystemLicenseStore::put_license: temporary filename for Licence truncated: %s/%s/0.0.0.0_0x%08X_%s_%s_%s",
                license_path.c_str(), client_name, version, scope, company_name, product_id);
            return false;
        };

        PathMaker path_maker;
        if (!path_maker.push_dir(license_path, chars_view{client_name, strlen(client_name)})) {
            return truncated_error();
        }

        if (recursive_create_directory(path_maker.path, S_IRWXU | S_IRWXG) != 0) {
            LOG(LOG_ERR, "FileSystemLicenseStore::put_license(): Failed to create directory: \"%s\"", path_maker.path);
            return false;
        }

        if (!path_maker.push_filename(enable_log, "put_license", "0.0.0.0_", version, scope, company_name, product_id)) {
            return truncated_error();
        }

        auto end_filename = path_maker.it;

        if (!path_maker.push_template()) {
            return truncated_error();
        }

        auto io_error = [&](char const* msg){
            int errnum = errno;
            LOG(LOG_ERR,
                "FileSystemLicenseStore::put_license: %s filename=\"%s\" errno=%s(%d)",
                msg, path_maker.path, strerror(errnum), errnum
            );
            return false;
        };

        int fd = ::mkostemps(path_maker.path, 4, O_CREAT | O_WRONLY);
        if (fd == -1) {
            return io_error("Failed to open (temporary) license file for writing!");
        }
        unique_fd ufd{fd};

        if (hwid.size() != ::write(ufd.fd(), hwid.data(), hwid.size())) {
            return io_error("Failed to write (temporary) license file!");
        }

        uint32_t const license_size = in.size();
        if (sizeof(license_size) != ::write(ufd.fd(), &license_size, sizeof(license_size))
         || license_size != ::write(ufd.fd(), in.data(), in.size())
        ) {
            return io_error("Failed to write (temporary) license file size!");
        }

        char newpath[PATH_MAX];
        *byte_copy(newpath, {path_maker.path, end_filename}) = '\0';
        if (-1 == rename(path_maker.path, newpath)) {
            io_error("Failed to rename the (temporary) license file!");
            *end_filename = '-';
            unlink(path_maker.path);
            return false;
        }

        return true;
    }

private:
    std::string license_path;

    struct PathMaker
    {
        char path[PATH_MAX];
        char* it = path;

        bool push_dir(chars_view license_path, chars_view client_name)
        {
            std::size_t const inserted_chars = 10; // '/', '\0' and others
            if (std::size(path) < license_path.size() + client_name.size() + inserted_chars) {
                return false;
            }
            it = byte_copy(it, license_path);
            *it++ = '/';
            auto start_client = it;
            it = std::copy_if(client_name.begin(), client_name.end(), it, [](char c){
                return c != '/';
            });
            // when "", "." or ".."
            if (it == start_client
            || (it == start_client + 1 && *it == '.')
            || (it == start_client + 2 && it[0] == '.' && it[1] == '.')
            ) {
                it = start_client;
                *it++ = 'x';
                *it++ = 'y';
            }
            *it++ = '/';
            *it = '\0';

            return true;
        }

        bool push_filename(
            bool enable_log, char const* funcname, char const* prefix,
            uint32_t version, char const* scope,
            char const* company_name, char const* product_id)
        {
            auto remaining = static_cast<std::size_t>(std::end(path) - it) - 1;
            int n = snprintf(
                it, remaining, "%s0x%08X_%s_%s_%s",
                prefix, version, scope, company_name, product_id
            );
            if (n < 0 || n >= static_cast<int>(remaining)) {
                return false;
            }
            auto end_index = std::remove(it, it + n, '/');
            std::replace(it, end_index, ' ', '-');
            *end_index = '\0';
            LOG_IF(enable_log, LOG_INFO, "FileSystemLicenseStore::%s(): LicenseIndex=\"%s\"", funcname, it);
            it = end_index;

            return true;
        }

        bool push_template()
        {
            auto templ = "-XXXXXX.tmp"_av;
            if (static_cast<std::size_t>(std::end(path) - it) < templ.size() + 1) {
                return false;
            }
            it = byte_copy(it, templ);
            *it = '\0';
            return true;
        }
    };
};
