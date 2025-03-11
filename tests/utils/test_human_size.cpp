/*
SPDX-FileCopyrightText: 2025 Wallix Proxies Team
SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "test_only/test_framework/redemption_unit_tests.hpp"

#include "utils/human_size.hpp"

#include <string>


RED_AUTO_TEST_CASE(TestHumanSizePowerOf2)
{
    constexpr uint64_t K = 1024;
    constexpr uint64_t M = K * 1024;
    constexpr uint64_t G = M * 1024;

    std::string buffer;
    auto human_size = [&](uint64_t n) {
        HumanSizePowerOf2 h {n, '.'};
        buffer.assign(h.sv().begin(), h.sv().end());
        static constexpr char const * units[]{
            " B",
            " KiB",
            " MiB",
            " GiB",
            " TiB",
            " PiB",
            " EiB",
        };
        buffer += units[underlying_cast(h.unit())];
        return chars_view(buffer);
    };

    RED_CHECK(human_size(0) == "0 B"_av);
    RED_CHECK(human_size(9) == "9 B"_av);
    RED_CHECK(human_size(10) == "10 B"_av);
    RED_CHECK(human_size(11) == "11 B"_av);
    RED_CHECK(human_size(100) == "100 B"_av);
    RED_CHECK(human_size(300) == "300 B"_av);
    RED_CHECK(human_size(600) == "600 B"_av);
    RED_CHECK(human_size(900) == "900 B"_av);
    RED_CHECK(human_size(1000) == "1000 B"_av);
    RED_CHECK(human_size(1023) == "1023 B"_av);

    RED_CHECK(human_size(K + 0) == "1.00 KiB"_av);
    RED_CHECK(human_size(K + 1023) == "1.99 KiB"_av);
    RED_CHECK(human_size(K + 1) == "1.00 KiB"_av);
    RED_CHECK(human_size(K + 11) == "1.01 KiB"_av);
    RED_CHECK(human_size(K + 111) == "1.10 KiB"_av);
    RED_CHECK(human_size(K + 1000) == "1.97 KiB"_av);
    RED_CHECK(human_size(K * 2 - 1) == "1.99 KiB"_av);

    RED_CHECK(human_size(20*K) == "20.00 KiB"_av);
    RED_CHECK(human_size(100*K) == "100.00 KiB"_av);
    RED_CHECK(human_size(500*K) == "500.00 KiB"_av);
    RED_CHECK(human_size(500*K + 500) == "500.48 KiB"_av);
    RED_CHECK(human_size(900*K) == "900.00 KiB"_av);
    RED_CHECK(human_size(1000*K) == "1000.00 KiB"_av);
    RED_CHECK(human_size(1013*K) == "1013.00 KiB"_av);
    RED_CHECK(human_size(1023*K) == "1023.00 KiB"_av);
    RED_CHECK(human_size(1023*K + 333) == "1023.32 KiB"_av);
    RED_CHECK(human_size(M - 1) == "1023.99 KiB"_av);

    RED_CHECK(human_size(M + K*0) == "1.00 MiB"_av);
    RED_CHECK(human_size(M + K*1) == "1.00 MiB"_av);
    RED_CHECK(human_size(M + K*11) == "1.01 MiB"_av);
    RED_CHECK(human_size(M + K*111) == "1.10 MiB"_av);
    RED_CHECK(human_size(M + K*1000) == "1.97 MiB"_av);
    RED_CHECK(human_size(M + K*1023) == "1.99 MiB"_av);

    RED_CHECK(human_size(20*M) == "20.00 MiB"_av);
    RED_CHECK(human_size(100*M) == "100.00 MiB"_av);
    RED_CHECK(human_size(500*M) == "500.00 MiB"_av);
    RED_CHECK(human_size(500*M + 500*K) == "500.48 MiB"_av);
    RED_CHECK(human_size(900*M) == "900.00 MiB"_av);
    RED_CHECK(human_size(1000*M) == "1000.00 MiB"_av);
    RED_CHECK(human_size(1013*M) == "1013.00 MiB"_av);
    RED_CHECK(human_size(1023*M) == "1023.00 MiB"_av);
    RED_CHECK(human_size(1023*M + 333*K) == "1023.32 MiB"_av);
    RED_CHECK(human_size(G - 1) == "1023.99 MiB"_av);

    RED_CHECK(human_size(G + M*0) == "1.00 GiB"_av);
    RED_CHECK(human_size(G + M*1) == "1.00 GiB"_av);
    RED_CHECK(human_size(G + M*11) == "1.01 GiB"_av);
    RED_CHECK(human_size(G + M*111) == "1.10 GiB"_av);
    RED_CHECK(human_size(G + M*1000) == "1.97 GiB"_av);
    RED_CHECK(human_size(G + M*1023) == "1.99 GiB"_av);

    RED_CHECK(human_size(20*G) == "20.00 GiB"_av);
    RED_CHECK(human_size(100*G) == "100.00 GiB"_av);
    RED_CHECK(human_size(500*G) == "500.00 GiB"_av);
    RED_CHECK(human_size(500*G + 500*M) == "500.48 GiB"_av);
    RED_CHECK(human_size(900*G) == "900.00 GiB"_av);
    RED_CHECK(human_size(1000*G) == "1000.00 GiB"_av);
    RED_CHECK(human_size(1023*G) == "1023.00 GiB"_av);
    RED_CHECK(human_size(1023*G + 333*M) == "1023.32 GiB"_av);
}
