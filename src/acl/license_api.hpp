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

#include "core/RDP/lic.hpp"
#include "utils/sugar/bytes_view.hpp"
#include "utils/sugar/bounded_bytes_view.hpp"
#include "utils/sugar/noncopyable.hpp"

struct LicenseApi : noncopyable
{
    struct LicenseInfo
    {
        uint32_t version;
        chars_view client_name;
        chars_view scope;
        chars_view company_name;
        chars_view product_id;
    };

    virtual ~LicenseApi() = default;

    // The functions shall return empty bytes_view to indicate the error.
    virtual bytes_view get_license_v1(
        LicenseInfo license_info,
        writable_sized_bytes_view<LIC::LICENSE_HWID_SIZE> hwid,
        writable_bytes_view out, bool enable_log) = 0;

    // The functions shall return empty bytes_view to indicate the error.
    virtual bytes_view get_license_v0(
        LicenseInfo license_info, writable_bytes_view out, bool enable_log) = 0;

    virtual bool put_license(
        LicenseInfo license_info,
        sized_bytes_view<LIC::LICENSE_HWID_SIZE> hwid,
        bytes_view in, bool enable_log) = 0;
};

struct NullLicenseStore : LicenseApi
{
    bytes_view get_license_v1(
        LicenseInfo license_info, writable_sized_bytes_view<LIC::LICENSE_HWID_SIZE> hwid,
        writable_bytes_view out, bool enable_log) override
    {
        (void)license_info;
        (void)hwid;
        (void)out;
        (void)enable_log;

        return {};
    }

    bytes_view get_license_v0(
        LicenseInfo license_info,
        writable_bytes_view out, bool enable_log) override
    {
        (void)license_info;
        (void)out;
        (void)enable_log;

        return {};
    }

    bool put_license(
        LicenseInfo license_info,
        sized_bytes_view<LIC::LICENSE_HWID_SIZE> hwid,
        bytes_view in, bool enable_log) override
    {
        (void)license_info;
        (void)hwid;
        (void)in;
        (void)enable_log;

        return false;
    }
};
