/*
SPDX-FileCopyrightText: 2025 Wallix Proxies Team
SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "test_only/test_framework/redemption_unit_tests.hpp"

#include "utils/eta.hpp"


RED_AUTO_TEST_CASE(TestETA)
{
    using namespace std::chrono_literals;

    uint64_t total = 100'000'000;

    using Duration = std::chrono::seconds;
    using Time = Eta::Time;

    Eta eta_ctx;
    eta_ctx.start(Time{});

    auto eta = [&](Eta::Sample sample) {
        eta_ctx.update(sample);
        return std::chrono::duration_cast<Duration>(eta_ctx.compute_eta(total));
    };

    auto duration_max = std::chrono::duration_cast<Duration>(Eta::Duration::max());

    RED_TEST_CONTEXT("0 sample")
    {
        RED_CHECK(eta_ctx.compute_eta(total) == Eta::Duration::max());
    }

    RED_TEST_CONTEXT("1 sample")
    {
        RED_CHECK(eta({.time{ 3s }, .value = 1000}) == 300000s);
    }

    RED_TEST_CONTEXT("no data on 8 samples")
    {
        RED_CHECK(eta({.time{ 4s }, .value = 1000}) == 400000s);
        RED_CHECK(eta({.time{ 5s }, .value = 1000}) == 499999s);
        RED_CHECK(eta({.time{ 6s }, .value = 1000}) == 600000s);
        RED_CHECK(eta({.time{ 7s }, .value = 1000}) == 700000s);
        RED_CHECK(eta({.time{ 8s }, .value = 1000}) == 800000s);
        RED_CHECK(eta({.time{ 9s }, .value = 1000}) == 899999s);
        RED_CHECK(eta({.time{ 10s }, .value = 1000}) == duration_max);
        RED_CHECK(eta({.time{ 11s }, .value = 1000}) == duration_max);
        RED_CHECK(eta({.time{ 12s }, .value = 1000}) == duration_max);
    }

    RED_TEST_CONTEXT("random samples")
    {
        RED_CHECK(eta({.time{ 14s }, .value =  13'000}) == 88887s);
        RED_CHECK(eta({.time{ 17s }, .value = 113'000}) == 11494s);
        RED_CHECK(eta({.time{ 18s }, .value = 123'000}) == 8315s);
        RED_CHECK(eta({.time{ 20s }, .value = 183'000}) == 6321s);
        RED_CHECK(eta({.time{ 21s }, .value = 198'000}) == 5481s);
        RED_CHECK(eta({.time{ 22s }, .value = 250'000}) == 4481s);
        RED_CHECK(eta({.time{ 24s }, .value = 287'000}) == 4206s);
        RED_CHECK(eta({.time{ 25s }, .value = 322'000}) == 3517s);
        RED_CHECK(eta({.time{ 27s }, .value = 358'000}) == 3692s);
        RED_CHECK(eta({.time{ 28s }, .value = 388'000}) == 3717s);
        RED_CHECK(eta({.time{ 29s }, .value = 418'000}) == 3670s);
        RED_CHECK(eta({.time{ 30s }, .value = 458'000}) == 3520s);
        RED_CHECK(eta({.time{ 32s }, .value = 506'000}) == 3897s);
        RED_CHECK(eta({.time{ 34s }, .value = 541'000}) == 3915s);
        RED_CHECK(eta({.time{ 35s }, .value = 579'000}) == 3887s);
        RED_CHECK(eta({.time{ 38s }, .value = 615'000}) == 4264s);
        RED_CHECK(eta({.time{ 39s }, .value = 1'315'000}) == 1442s);
        RED_CHECK(eta({.time{ 40s }, .value = 2'015'000}) == 766s);
        RED_CHECK(eta({.time{ 41s }, .value = 3'815'000}) == 370s);
        RED_CHECK(eta({.time{ 42s }, .value = 5'415'000}) == 216s);
        RED_CHECK(eta({.time{ 43s }, .value = 9'115'000}) == 114s);
        RED_CHECK(eta({.time{ 44s }, .value = 14'915'000}) == 68s);
        RED_CHECK(eta({.time{ 45s }, .value = 30'701'000}) == 26s);
        RED_CHECK(eta({.time{ 46s }, .value = 44'621'000}) == 16s);
        RED_CHECK(eta({.time{ 47s }, .value = 59'821'000}) == 12s);
        RED_CHECK(eta({.time{ 48s }, .value = 76'014'000}) == 9s);
        RED_CHECK(eta({.time{ 49s }, .value = 80'342'000}) == 8s);
        RED_CHECK(eta({.time{ 50s }, .value = 96'582'000}) == 7s);
        RED_CHECK(eta({.time{ 51s }, .value = 100'000'000}) == 6s);
    }

    RED_TEST_CONTEXT("samples")
    {
        eta_ctx.start(Time{});
        RED_CHECK(eta({.time{ 1s }, .value = 5'000'000}) == 20s);
        RED_CHECK(eta({.time{ 2s }, .value = 20'000'000}) == 12s);
        RED_CHECK(eta({.time{ 3s }, .value = 25'000'000}) == 12s);
        RED_CHECK(eta({.time{ 4s }, .value = 40'000'000}) == 10s);
        RED_CHECK(eta({.time{ 5s }, .value = 45'000'000}) == 11s);
        RED_CHECK(eta({.time{ 6s }, .value = 60'000'000}) == 10s);
        RED_CHECK(eta({.time{ 7s }, .value = 65'000'000}) == 10s);
        RED_CHECK(eta({.time{ 8s }, .value = 80'000'000}) == 9s);
        RED_CHECK(eta({.time{ 9s }, .value = 85'000'000}) == 8s);
        RED_CHECK(eta({.time{ 10s }, .value = 100'000'000}) == 7s);
    }
}
