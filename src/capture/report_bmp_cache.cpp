/*
SPDX-FileCopyrightText: 2025 Wallix Proxies Team

SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "capture/report_bmp_cache.hpp"
#include "capture/save_state_chunk.hpp"
#include "core/error.hpp"
#include "core/RDP/bitmapupdate.hpp"
#include "core/RDP/MonitorLayoutPDU.hpp"
#include "core/RDP/orders/RDPOrdersSecondaryFrameMarker.hpp"
#include "core/RDP/state_chunk.hpp"
#include "utils/file.hpp"

ReportBmpCache::ReportBmpCache(Transport & trans)
    : trans(&trans)
    , compression_builder(trans, WrmCompressionAlgorithm::no_compression)
{
}

int ReportBmpCache::process(const std::string& output_filename)
{
    if (File output_file { output_filename, "w" }) {
        fprintf(output_file.get(), "put,get,time,perfect\n");
        while (next_order()){
            this->interpret_order(output_file.get());
        }
    }
    else {
        LOG(LOG_ERR, "Failed to open output file '%s' for redirection: %s", output_filename.c_str(), strerror(errno));
        return 1;
    }

    if(this->cache_put_count == 0){
        LOG(LOG_ERR, "No put in cache.");
        return 1;
    }

    std::string output_sh_filename = output_filename + ".sh";

    if (File script_file { output_sh_filename, "w" }) {
        fprintf(script_file.get(),
            "#!/usr/bin/env gnuplot\n"
            "set terminal pngcairo size 1280,720;\n"
            "set output '%s.png';\n"
            "set title 'Graph de PUT, GET et Perfect Cache en fonction du temps';\n"
            "set xlabel 'Temps (µs)';\n"
            "set ylabel 'Valeurs';\n"
            "set grid;\n"
            "set datafile separator ',';\n"
            "set key left top;\n"
            "plot '%s' using 3:1 skip 1 with linespoints title 'PUT', "
            "'%s' using 3:2 skip 1 with linespoints title 'GET', "
            "'%s' using 3:4 skip 1 with linespoints title 'Perfect Cache Size'\n",
            output_filename.c_str(),
            output_filename.c_str(),
            output_filename.c_str(),
            output_filename.c_str()
        );
    }
    else{
        LOG(LOG_ERR, "Failed to create the '%s' script file: %s", output_sh_filename.c_str(), strerror(errno));
        return 1;
    }

    return 0;
}


void ReportBmpCache::print_order_state(std::FILE * output_file)
{
      if(this->first_record_now < MonotonicTimePoint()){
        this->first_record_now = record_now;
    }
    assert(output_file);
    fprintf(output_file,
        "%lu,%lu,%lld,%zu\n",
        cache_put_count,
        cache_get_count,
        static_cast<long long>(std::chrono::duration_cast<std::chrono::microseconds>(this->record_now - this->first_record_now).count()),
        perfect_cache.size()
    );
}

bool ReportBmpCache::next_order()
{
    if (this->chunk_type != WrmChunkType::LAST_IMAGE_CHUNK
     && this->chunk_type != WrmChunkType::PARTIAL_IMAGE_CHUNK
    ) {
        if (this->stream.get_current() == this->stream.get_data_end()
         && this->remaining_order_count
        ) {
            LOG(LOG_ERR, "Incomplete order batch at chunk %u "
                         "order [%lu/%lu] remaining [%zu/%u]",
                         this->chunk_type,
                         this->chunk_count - this->remaining_order_count,
                         this->chunk_count,
                         this->stream.in_remain(),
                         this->chunk_size);
            throw Error(ERR_WRM);
        }
    }

    if (!this->remaining_order_count){

        uint8_t buf[WRM_HEADER_SIZE];
        if (Transport::Read::Eof == this->trans->atomic_read(buf, WRM_HEADER_SIZE)){
            return false;
        }
        InStream header(buf);
        this->chunk_type = safe_cast<WrmChunkType>(header.in_uint16_le());
        this->chunk_size = header.in_uint32_le();
        this->remaining_order_count = this->chunk_count = header.in_uint16_le();

        if (this->chunk_size > 65536){
            LOG(LOG_ERR,"chunk_size (%u) > 65536", this->chunk_size);
            throw Error(ERR_WRM);
        }
        this->stream = InStream(this->stream_buf);
        if (this->chunk_size - WRM_HEADER_SIZE > 0) {
            auto av = this->trans->recv_boom(
                this->stream_buf, this->chunk_size - WRM_HEADER_SIZE);
            this->stream = InStream(av);
        }
    }

    if (this->remaining_order_count > 0) {
        this->remaining_order_count--;
    }
    return true;
}

void ReportBmpCache::interpret_order(std::FILE * output_file)
{
    this->total_orders_count++;
    auto const & palette = BGRPalette::classic_332();

    REDEMPTION_DIAGNOSTIC_PUSH()
    REDEMPTION_DIAGNOSTIC_CLANG_IGNORE("-Wcovered-switch-default")
    REDEMPTION_DIAGNOSTIC_GCC_IGNORE("-Wswitch")
    switch (this->chunk_type)
    {
        case WrmChunkType::RDP_UPDATE_ORDERS:
        {
            if (!this->meta_ok){
                LOG(LOG_ERR, "Drawing orders chunk must be preceded by a META chunk to get drawing device size");
                throw Error(ERR_WRM);
            }
            uint8_t control = this->stream.in_uint8();
            uint8_t class_ = (control & (RDP::STANDARD | RDP::SECONDARY));
            if (class_ == RDP::SECONDARY) {
                RDP::AltsecDrawingOrderHeader header(control);
                REDEMPTION_DIAGNOSTIC_GCC_IGNORE("-Wswitch-enum")
                switch (header.orderType) {
                    case RDP::AltsecDrawingOrderType::FrameMarker:
                    {
                        RDP::FrameMarker frame_marker;
                        frame_marker.receive(this->stream, header);

                    }
                    break;
                    case RDP::AltsecDrawingOrderType::Window:
                    {
                        std::size_t len = this->stream.clone().in_uint16_le();
                        this->stream.in_skip_bytes(std::min(len, this->stream.in_remain()));
                    }
                    break;
                    default:
                        LOG(LOG_WARNING, "unsupported Alternate Secondary Drawing Order (%d)", header.orderType);
                        /* error, unknown order */
                    break;
                }
            }
            else if (class_ == (RDP::STANDARD | RDP::SECONDARY)) {
                RDPSecondaryOrderHeader header(this->stream);
                uint8_t const *next_order = this->stream.get_current() + header.order_data_length();
                switch (header.type) {
                case RDP::TS_CACHE_BITMAP_COMPRESSED:
                case RDP::TS_CACHE_BITMAP_UNCOMPRESSED:
                {
                    RDPBmpCache order;
                    BitsPerPixel bpp{0};
                    order.receive(stream, header, palette, bpp);
                    cache_put_count++;

                    auto it = this->perfect_cache.find(order.bmp);
                    if (it == this->perfect_cache.end()) {
                        this->perfect_cache.insert(order.bmp);
                    }else{
                        this->already_in_cache++;
                    }

                    this->bmp_cache->put(order.id, order.idx, order.bmp, order.key1, order.key2);
                    this->print_order_state(output_file);
                }
                break;
                default:
                    LOG(LOG_ERR, "unsupported SECONDARY ORDER (%u)", header.type);
                    /* error, unknown order */
                    break;
                }
                this->stream.in_skip_bytes(next_order - this->stream.get_current());
            }
            else if (class_ == RDP::STANDARD) {
                RDPPrimaryOrderHeader header = this->common.receive(this->stream, control);
                switch (this->common.order) {
                case RDP::MEMBLT:
                    {
                        this->memblt.receive(stream, header);
                        const Bitmap & bmp = this->bmp_cache->get(
                            this->memblt.cache_id, this->memblt.cache_idx);
                        cache_get_count++;

                        this->print_order_state(output_file);
                        if (!bmp.is_valid()){
                            LOG(LOG_ERR, "Memblt bitmap not found in cache at (%u, %u)",
                                this->memblt.cache_id, this->memblt.cache_idx);
                            throw Error(ERR_WRM);
                        }
                    }
                    break;
                case RDP::MEM3BLT:
                    {
                        this->mem3blt.receive(stream, header);
                        const Bitmap & bmp = this->bmp_cache->get(
                            this->mem3blt.cache_id, this->mem3blt.cache_idx);
                        cache_get_count++;

                        this->print_order_state(output_file);
                        if (!bmp.is_valid()){
                            LOG(LOG_ERR, "Mem3blt bitmap not found in cache at (%u, %u)",
                                this->mem3blt.cache_id, this->mem3blt.cache_idx);
                            throw Error(ERR_WRM);
                        }
                    }
                    break;

                case RDP::GLYPHINDEX:
                      RDPGlyphIndex(
                          0, 0, 0, 0, RDPColor{}, RDPColor{}, Rect(0, 0, 1, 1),
                        Rect(0, 0, 1, 1), RDPBrush(), 0, 0, 0, byte_ptr_cast("")
                    ).receive(stream, header);
                    break;
                case RDP::DESTBLT:
                    RDPDstBlt(Rect(), 0).receive(stream, header);
                    break;
                case RDP::MULTIDSTBLT:
                    RDPMultiDstBlt().receive(stream, header);
                    break;
                case RDP::MULTIOPAQUERECT:
                    RDPMultiOpaqueRect().receive(stream, header);
                    break;
                case RDP::MULTIPATBLT:
                    RDP::RDPMultiPatBlt().receive(stream, header);
                    break;
                case RDP::MULTISCRBLT:
                    RDP::RDPMultiScrBlt().receive(stream, header);
                    break;
                case RDP::PATBLT:
                    RDPPatBlt(Rect(), 0, RDPColor{}, RDPColor{}, RDPBrush()).receive(stream, header);
                    break;
                case RDP::SCREENBLT:
                    RDPScrBlt(Rect(), 0, 0, 0).receive(stream, header);
                    break;
                case RDP::LINE:
                    RDPLineTo(0, 0, 0, 0, 0, RDPColor{}, 0, RDPPen(0, 0, RDPColor{})).receive(stream, header);
                    break;
                case RDP::RECT:
                    RDPOpaqueRect(Rect(), RDPColor{}).receive(stream, header);
                    break;
                case RDP::POLYLINE:
                    RDPPolyline().receive(stream, header);
                    break;
                case RDP::ELLIPSESC:
                    RDPEllipseSC().receive(stream, header);
                    break;
                default:
                    /* error unknown order */
                    LOG(LOG_ERR, "unsupported PRIMARY ORDER (%d)", this->common.order);
                    throw Error(ERR_WRM);
                }

            }
        }
        break;
        case WrmChunkType::TIMES:
        case WrmChunkType::TIMESTAMP_OR_RECORD_DELAY:
            this->record_now = MonotonicTimePoint(std::chrono::microseconds(this->stream.in_uint64_le()));
            this->stream.in_skip_remaining();
            break;
        case WrmChunkType::META_FILE:
        {
            WrmMetaChunk info{};
            info.receive(this->stream);
            this->trans = &this->compression_builder.reset(
                *this->trans_source, info.compression_algorithm
            );

            this->stream.in_skip_remaining();

            if (!this->meta_ok) {
                this->bmp_cache = std::make_unique<BmpCache>(
                    BmpCache::Recorder, info.bpp,
                    info.number_of_cache,
                    info.use_waiting_list,
                    BmpCache::CacheOption(
                        info.cache_0_entries, info.cache_0_size, info.cache_0_persistent),
                    BmpCache::CacheOption(
                        info.cache_1_entries, info.cache_1_size, info.cache_1_persistent),
                    BmpCache::CacheOption(
                        info.cache_2_entries, info.cache_2_size, info.cache_2_persistent),
                    BmpCache::CacheOption(
                        info.cache_3_entries, info.cache_3_size, info.cache_3_persistent),
                    BmpCache::CacheOption(
                        info.cache_4_entries, info.cache_4_size, info.cache_4_persistent),
                    BmpCache::Verbose::none);
                this->meta_ok = true;

                printf("\nWRM file version      : %d", static_cast<int>(info.version));
                printf("\nWidth                 : %d", info.width);
                printf("\nHeight                : %d", info.height);
                printf("\nBpp                   : %d", static_cast<int>(info.bpp));
                printf("\nCache 0 entries       : %d", info.cache_0_entries);
                printf("\nCache 0 size          : %d", info.cache_0_size);
                printf("\nCache 1 entries       : %d", info.cache_1_entries);
                printf("\nCache 1 size          : %d", info.cache_1_size);
                printf("\nCache 2 entries       : %d", info.cache_2_entries);
                printf("\nCache 2 size          : %d", info.cache_2_size);
                printf("\n");
            }
        }
        break;

        case WrmChunkType::LAST_IMAGE_CHUNK:
        case WrmChunkType::PARTIAL_IMAGE_CHUNK:
        {
            this->remaining_order_count = 0;
        }
        break;

        case WrmChunkType::RDP_UPDATE_BITMAP:
        case WrmChunkType::RDP_UPDATE_BITMAP2:
        {
            RDPBitmapData order;
            order.receive(stream);
            this->stream.in_skip_bytes(order.bitmap_size());
        }
        break;


        case WrmChunkType::POINTER_NATIVE:
        {
            this->stream.in_uint32_le(); // Skip BitsPerPixel and cached idx
        }
        break;



        case WrmChunkType::MONITOR_LAYOUT:
        {
                ::check_throw(stream, 4,
                          "MonitorLayoutPDU::recv",
                          ERR_RDP_DATA_TRUNCATED);


                uint32_t monitorCount{stream.in_uint32_le()};

                ::check_throw(stream, monitorCount * 20, // monitorCount * monitorDefArray(20)
                        "MonitorLayoutPDU::recv",
                        ERR_RDP_DATA_TRUNCATED);
                this->stream.in_skip_bytes(monitorCount * 20);
        }
        break;


        case WrmChunkType::KBD_INPUT_MASK:
        {
            this->stream.in_skip_bytes(1); // Skip enabled state
        }
        break;


        case WrmChunkType::OLD_SESSION_UPDATE:
        case WrmChunkType::SESSION_UPDATE:
        {
            this->stream.in_skip_bytes(8); // Skip MonotonicTimePoint
            this->stream.in_skip_bytes(this->stream.in_uint16_le()); // Skip fixed message
        }
        break;


        case WrmChunkType::POINTER:
        case WrmChunkType::POINTER2:
        case WrmChunkType::INTERNAL_POINTER:
        {
            this->stream.in_skip_bytes(5); // Skip mouse x, y and cached idx
        }
        break;

        case WrmChunkType::POSSIBLE_ACTIVE_WINDOW_CHANGE:
        break;

        case WrmChunkType::RESET_CHUNK:
            this->trans = this->trans_source;
        break;

        case WrmChunkType::SAVE_STATE:
        {
              StateChunk state;
            SaveStateChunk ssc;
            ssc.recv(this->stream, state, this->version);
        }
        break;

        case WrmChunkType::RAIL_WINDOW_RECT:
        {
            this->stream.in_sint64_le(); // Skip windows rect x, y, cx and cy
        }
        break;

        case WrmChunkType::IMAGE_FRAME_RECT:
        {
            this->stream.in_sint64_le(); // Skip windows rect x, y, cx and cy
            if (this->stream.in_remain() >= 4) {
                this->stream.in_uint32_le(); // Skip w and h
            }
        }
        break;

        case WrmChunkType::INVALID_CHUNK:
        default:
            LOG(LOG_ERR, "FileToGraphic: unknown chunk type %d", this->chunk_type);
            throw Error(ERR_WRM);
    }
    REDEMPTION_DIAGNOSTIC_POP()
}