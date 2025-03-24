/*
SPDX-FileCopyrightText: 2025 Wallix Proxies Team

SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "utils/stream.hpp"
#include "utils/compression_transport_builder.hpp"
#include "capture/wrm_chunk_type.hpp"
#include "capture/wrm_meta_chunk.hpp"
#include "utils/monotonic_clock.hpp"
#include "core/RDP/caches/bmpcache.hpp"
#include "core/RDP/orders/RDPOrdersCommon.hpp"
#include "core/RDP/orders/RDPOrdersPrimaryMemBlt.hpp"
#include "core/RDP/orders/RDPOrdersPrimaryMem3Blt.hpp"
#include <unordered_set>
#include <string>
#include <string_view>

class Transport;
class BmpCache;
class Order;

namespace gdi
{
    class GraphicApi;
}

class ReportBmpCache
{
public:
    ReportBmpCache(Transport & trans);
    int process(const std::string& output_filename);
    bool next_order();
    void interpret_order(std::FILE * output_file);
private:
    void print_order_state(std::FILE * output_file);

    static std::string_view bmp_to_sv(Bitmap const & bmp)
    {
        return std::string_view(char_ptr_cast(bmp.data()), bmp.bmp_size());
    }

    struct bmp_hash
    {
        std::size_t operator () (Bitmap const & bmp) const
        {
            std::hash<std::string_view> hasher;
            return hasher(bmp_to_sv(bmp));
        }
    };

    struct bmp_equal
    {
        bool operator () (Bitmap const & bmp1, Bitmap const & bmp2) const
        {
            return bmp_to_sv(bmp1) == bmp_to_sv(bmp2);
        }
    };

    Transport * trans_source;
    Transport * trans;
    RDPOrderCommon common{RDP::PATBLT, Rect(0, 0, 1, 1)};
    RDPMemBlt memblt{0, Rect(), 0, 0, 0, 0};
    RDPMem3Blt mem3blt{0, Rect(), 0, 0, 0, RDPColor{}, RDPColor{}, RDPBrush(), 0};
    uint8_t stream_buf[65536];
    InStream stream {stream_buf};

    CompressionInTransportBuilder compression_builder;

    bool meta_ok = false;

    uint32_t chunk_size = 0;
    WrmChunkType chunk_type = WrmChunkType::INVALID_CHUNK;
    uint64_t chunk_count = 0;
    uint64_t remaining_order_count = 0;

    uint32_t total_orders_count = 0;

    uint8_t version = 0;
    MonotonicTimePoint record_now {};
    std::unique_ptr<BmpCache> bmp_cache;

    uint64_t cache_put_count = 0;
    uint64_t cache_get_count = 0;
    uint64_t already_in_cache = 0;

    MonotonicTimePoint first_record_now {std::chrono::milliseconds(-1)};

    std::unordered_set<Bitmap, bmp_hash, bmp_equal> perfect_cache;

};
