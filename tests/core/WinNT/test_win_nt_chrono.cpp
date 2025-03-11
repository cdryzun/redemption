/*
SPDX-FileCopyrightText: 2025 Wallix Proxies Team
SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "test_only/test_framework/redemption_unit_tests.hpp"

#include "core/WinNT/chrono.hpp"

RED_AUTO_TEST_CASE(TestWinNtChrono)
{
    using namespace std::chrono;

    auto const unix_file_time = sys_days{2025y / April / 23d} + 17h + 21min;
    auto const ms_file_time = clock_cast<WinNtClock>(unix_file_time);
    RED_CHECK(unix_file_time.time_since_epoch() == 1745428860s);
    RED_CHECK(ms_file_time.time_since_epoch() ==  13389902460s);
    RED_CHECK(duration_cast<WinNtDuration>(ms_file_time.time_since_epoch()).count()
        == 13389902460'0000000); // 100-nanosecond
    auto const to_sys = clock_cast<system_clock>(ms_file_time);
    RED_CHECK(to_sys == unix_file_time);
    auto const to_ms_sys = clock_cast<WinNtClock>(unix_file_time);
    RED_CHECK(to_ms_sys.time_since_epoch() == ms_file_time.time_since_epoch());
}
