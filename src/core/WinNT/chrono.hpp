/*
SPDX-FileCopyrightText: 2025 Wallix Proxies Team
SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <chrono>
#include "core/WinNT/time.hpp"

#include <ctime>  // clock_gettime


// number of 100-nanosecond intervals that have elapsed
// since 12:00 A.M. January 1, 1601 Coordinated Universal Time.
struct WinNtClock;

template<class Duration>
using WinNtTime = std::chrono::time_point<WinNtClock, Duration>;

using WinNtDuration = std::chrono::duration<int64_t, std::ratio<1, 10'000'000>>;

inline
WinNtTime<std::chrono::duration<std::underlying_type_t<WinNtUTime>, WinNtDuration::period>>
to_win_nt_time(WinNtUTime t) noexcept
{
    return WinNtTime<std::chrono::duration<std::underlying_type_t<WinNtUTime>, WinNtDuration::period>>{
        std::chrono::duration<std::underlying_type_t<WinNtUTime>, WinNtDuration::period>(
            static_cast<std::underlying_type_t<WinNtUTime>>(t)
       )
    };
}

inline WinNtUTime to_win_nt_utime(WinNtTime<WinNtDuration> tp) noexcept
{
    return WinNtUTime(tp.time_since_epoch().count());
}

// TODO rename to FastWinNtClock (a WinNtClock without leap seconds asjustement)
struct WinNtClock
{
    using rep = int64_t;
    using period = WinNtDuration::period;
    using duration = WinNtDuration;
    using time_point = std::chrono::time_point<WinNtClock, duration>;

    static const bool is_steady = false;

    static time_point now() noexcept
    {
        // return from_utc(std::chrono::utc_clock::now());

        timespec tp {};
        // without leap seconds
        clock_gettime(CLOCK_REALTIME, &tp);
        auto now = std::chrono::seconds(tp.tv_sec)
                 + std::chrono::nanoseconds(tp.tv_nsec);
        using CDur = LimitCommonDuration<decltype(now)>;
        auto file_time = duration_cast<CDur>(now);
        return time_point{ WinNtTime<CDur>{ms_file_time_epoch_adjustment} + file_time };
    }

// private:
    // possible overflow, limit ratio to period
    template<class Duration, class CDur = std::common_type_t<Duration, std::chrono::seconds>>
    using LimitCommonDuration = std::conditional_t<
        std::ratio_less_v<typename CDur::period, typename WinNtClock::period>,
        WinNtDuration, CDur
    >;

    // from MSSTL
    //@{
    // difference from 1 January 1970 00:00:00 (UNIX epoch)
    static constexpr std::chrono::seconds ms_file_time_epoch_adjustment{11'644'473'600};

    // Assumes that FILETIME counts leap seconds since 2018-06-01
    // (i.e., after the first 27 leap seconds), even though systems
    // can opt out of this behavior.
    static constexpr std::chrono::seconds skipped_filetime_leap_seconds{27};
    static constexpr std::chrono::sys_days cutoff{std::chrono::year_month_day{
        std::chrono::year{2018},
        std::chrono::June,
        std::chrono::day{1}
    }};
    //@}

#if 0
public:
    template<class Duration>
    static std::chrono::utc_time<std::common_type_t<Duration, std::chrono::seconds>>
    to_utc(WinNtTime<Duration> const& file_time)
    {
        using namespace std::chrono;

        using CDur = std::common_type_t<Duration, seconds>;

        const auto ticks = file_time.time_since_epoch() - ms_file_time_epoch_adjustment;

        if (ticks < cutoff.time_since_epoch())
        {
            return utc_clock::from_sys(sys_time<CDur>{ticks});
        }
        else
        {
            return utc_time<CDur>{ticks + skipped_filetime_leap_seconds};
        }
    }

    template<class Duration>
    static WinNtTime<LimitCommonDuration<Duration>>
    from_utc(std::chrono::utc_time<Duration> const& utc_time)
    {
        using namespace std::chrono;

        using CDur = LimitCommonDuration<Duration>;

        constexpr auto date_of_counts_leap_seconds
            = utc_seconds{cutoff.time_since_epoch()}
            + skipped_filetime_leap_seconds;

        auto file_time = (utc_time < date_of_counts_leap_seconds)
            ? duration_cast<CDur>(utc_clock::to_sys(utc_time).time_since_epoch())
            : duration_cast<CDur>(utc_time.time_since_epoch() - skipped_filetime_leap_seconds)
        ;

        return WinNtTime<CDur>{ms_file_time_epoch_adjustment} + file_time;
    }
#endif
};

inline
WinNtClock::time_point
to_win_nt_ignoring_leap_seconds(std::chrono::system_clock::time_point tp) noexcept
{
    using CDur = WinNtDuration;
    auto file_time = std::chrono::duration_cast<CDur>(tp.time_since_epoch());
    return WinNtTime<CDur>{ WinNtClock::ms_file_time_epoch_adjustment } + file_time;
}

inline
std::chrono::system_clock::time_point
to_sys_time_ignoring_leap_seconds(WinNtClock::time_point tp) noexcept
{
    using CDur = std::common_type_t<WinNtDuration, std::chrono::seconds>;
    const auto ticks = tp.time_since_epoch()
                     - std::chrono::duration_cast<CDur>(WinNtClock::ms_file_time_epoch_adjustment);
    return std::chrono::system_clock::time_point { ticks };
}
