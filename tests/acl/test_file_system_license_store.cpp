/*
SPDX-FileCopyrightText: 2024 Wallix Proxies Team
SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "test_only/test_framework/redemption_unit_tests.hpp"
#include "test_only/test_framework/file.hpp"
#include "test_only/test_framework/working_directory.hpp"

#include "acl/file_system_license_store.hpp"
#include "utils/strutils.hpp"
#include "utils/sugar/int_to_chars.hpp"

namespace
{
    constexpr LicenseApi::LicenseInfo license_info {
        .version = 3,
        // ' ' is replaced with '-' in filename
        .client_name = "Client /4"_av,
        // '/' is removed in filename
        .scope = "Scope /1"_av,
        .company_name = "Company /2"_av,
        .product_id = "ID /3"_av,
    };
}

RED_AUTO_TEST_CASE_WD(TestLicenseStoreV1, wd)
{
    FileSystemLicenseStore license_store(wd.dirname());
    auto subdir1 = wd.create_subdirectory("Client 4");
    auto license_file_name_v1 = subdir1.add_file("0.0.0.0_0x00000003_Scope-1_Company-2_ID-3");

    std::array<char, LIC::LICENSE_HWID_SIZE> hwid;
    uint8_t raw_buffer[128];
    auto out_buffer = make_writable_array_view(raw_buffer);

    /*
     * files not fount (v1 and v2)
     */

    RED_TEST(license_store.get_license_v1(license_info, make_writable_sized_array_view(hwid), out_buffer, false) == ""_av);
    RED_TEST(license_store.get_license_v0(license_info, out_buffer, false) == ""_av);

    /*
     * write license v1
     */

    RED_REQUIRE(license_store.put_license(license_info,
    "01234567890123456789"_sized_av, "my license data"_av, false));
    RED_TEST(RED_REQUIRE_GET_FILE_CONTENTS(license_file_name_v1) == "01234567890123456789\x0f\x00\x00\x00my license data"_av);

    /*
     * read license v1
     */

    RED_TEST(license_store.get_license_v1(license_info, make_writable_sized_array_view(hwid), out_buffer, false) == "my license data"_av);
    RED_TEST(hwid == "01234567890123456789"_av);

    /*
     * read license v0 -> not found
     */
    RED_TEST(license_store.get_license_v0(license_info, out_buffer, false) == ""_av);
}

RED_AUTO_TEST_CASE_WD(TestLicenseStoreV0, wd)
{
    FileSystemLicenseStore license_store(wd.dirname());
    auto subdir1 = wd.create_subdirectory("Client 4");
    auto license_file_name_v0 = subdir1.add_file("0x00000003_Scope-1_Company-2_ID-3");

    uint8_t raw_buffer[128];
    auto out_buffer = make_writable_array_view(raw_buffer);

    {
        unique_fd file{license_file_name_v0.c_str(), O_CREAT | O_WRONLY | O_TRUNC, 0600};
        RED_REQUIRE(file.is_open());
        auto content_v0 = "\x0f\x00\x00\x00my license data"_av;
        write(file.fd(), content_v0.data(), content_v0.size());
    }
    RED_TEST(license_store.get_license_v0(license_info, out_buffer, false) == "my license data"_av);
}
