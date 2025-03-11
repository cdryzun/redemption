/*
SPDX-FileCopyrightText: 2025 Wallix Proxies Team
SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <array>

#include "utils/monotonic_clock.hpp"


// Estimated time of arrival computer.
struct Eta
{
    using Time = MonotonicTimePoint;
    using Duration = Time::duration;

    struct Sample
    {
        Time time;
        uint64_t value;
    };

    Eta() = default;

    Eta(Time time) noexcept
    {
        start(time);
    }

    void start(Time time) noexcept
    {
        m_is_full = false;
        m_sample_cursor = 1;
        m_samples[0] = {time, 0};
    }

    void update(Sample sample) noexcept
    {
        if (!m_is_full && m_sample_cursor <= 1)
        {
            m_ema1 = sample.value;
            m_ema2 = sample.value;
        }
        // Apply Double Exponential Moving Average (DEMA) that is a
        // technique for smoothing time series for reduces the lag
        // associated with a simple moving average.
        // ema = a * s(i) + (1 - a) * s(i-1)
        // when a = 0.5 => (s(i) + s(i-1)) / 2
        m_ema1 = (sample.value + m_ema1) / 2;
        m_ema2 = (m_ema1 + m_ema2) / 2;
        sample.value = m_ema1 * 2 - m_ema2;

        // circular buffer of MAX_SAMPLE values
        m_samples[m_sample_cursor] = sample;
        ++m_sample_cursor;
        m_is_full = m_is_full || (m_sample_cursor == MAX_SAMPLE);
        m_sample_cursor %= MAX_SAMPLE;
    }

    Duration compute_eta(uint64_t total_value) const noexcept
    {
        auto ifirst = m_is_full ? m_sample_cursor : std::size_t{};
        auto ilast = (m_sample_cursor - 1u) % MAX_SAMPLE;
        auto const & first = m_samples[ifirst];
        auto const & last = m_samples[ilast];

        auto delta_value = last.value - first.value;
        auto delta_time = last.time - first.time;
        auto rm_emaining_value = total_value - last.value;

        // abort division by zero
        if (!delta_value)
        {
            return Duration::max();
        }

        auto to_f = [](auto v) { return static_cast<float>(v); };

        auto duration = to_f(rm_emaining_value + delta_value - 1u)
                      / to_f(delta_value)
                      * to_f(delta_time.count());
        return Duration(static_cast<Duration::rep>(duration));
    }

private:
    static const std::size_t MAX_SAMPLE = 8;

    std::array<Sample, MAX_SAMPLE> m_samples;
    uint64_t m_ema1;
    uint64_t m_ema2;
    uint8_t m_sample_cursor = 0;
    bool m_is_full;
};
