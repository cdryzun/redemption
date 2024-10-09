/*
  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

  Product name: redemption, a FLOSS RDP proxy
  Copyright (C) Wallix 2010
  Author(s): Christophe Grosjean, Javier Caverni, Dominique Lafages,
             Raphael Zhou, Meng Tan, Clément Moroldo
  Based on xrdp Copyright (C) Jay Sorg 2004-2010

  rdp module main header file
*/

#pragma once

#include "mod/rdp/channels/virtual_channel_filter.hpp"

class CliprdrWinXPMessageCompensator :
    public RemovableVirtualChannelFilter<CliprdrVirtualChannelProcessor>
{
public:
    CliprdrWinXPMessageCompensator(bool verbose) : verbose(verbose) {}

    void process_client_message(uint32_t total_length, uint32_t flags,
        bytes_view chunk_data, RDPECLIP::CliprdrHeader const* decoded_header) override
    {
        this->get_next_filter_ptr()->process_client_message(total_length,
            flags, chunk_data, decoded_header);
    }

    void process_server_message(uint32_t total_length, uint32_t flags,
        bytes_view chunk_data, RDPECLIP::CliprdrHeader const* decoded_header) override
    {
        InStream s(chunk_data);

        if (((flags & (CHANNELS::CHANNEL_FLAG_FIRST | CHANNELS::CHANNEL_FLAG_LAST)) == (CHANNELS::CHANNEL_FLAG_FIRST | CHANNELS::CHANNEL_FLAG_LAST)) &&
            (s.in_remain() >= 8 /* msgType(2) + msgFlags(2) + dataLen(4) */)) {

            const uint16_t msgType = s.in_uint16_le();
            if (RDPECLIP::CB_CLIP_CAPS == msgType) {
                this->server_clipboard_capabilities_pdu_sent = true;
            }
            else if (RDPECLIP::CB_MONITOR_READY == msgType) {
                LOG_IF(verbose, LOG_INFO,
                    "CliprdrWinXPMessageCompensator::process_server_message: CB_MONITOR_READY");

                if (!this->server_clipboard_capabilities_pdu_sent)
                {
                    LOG_IF(verbose, LOG_INFO,
                        "CliprdrWinXPMessageCompensator::process_server_message: CB_MONITOR_READY : Send default CB_CLIP_CAPS to client");

                    assert(this->get_previous_filter_ptr() != this);

                    RDPECLIP::GeneralCapabilitySet general_cap_set(
                        RDPECLIP::CB_CAPS_VERSION_1, 0);
                    RDPECLIP::ClipboardCapabilitiesPDU clipboard_caps_pdu(1);
                    RDPECLIP::CliprdrHeader caps_clipboard_header(
                        RDPECLIP::CB_CLIP_CAPS, RDPECLIP::CB_RESPONSE_NONE,
                        clipboard_caps_pdu.size() + general_cap_set.size());

                    StaticOutStream<128> caps_stream;

                    caps_clipboard_header.emit(caps_stream);
                    clipboard_caps_pdu.emit(caps_stream);
                    general_cap_set.emit(caps_stream);

                    this->get_previous_filter_ptr()->process_server_message(
                            caps_stream.get_offset(),
                            CHANNELS::CHANNEL_FLAG_FIRST
                            | CHANNELS::CHANNEL_FLAG_LAST
                            | CHANNELS::CHANNEL_FLAG_SHOW_PROTOCOL,
                            caps_stream.get_produced_bytes(),
                            &caps_clipboard_header
                        );
                }
                else
                {
                    this->server_clipboard_capabilities_pdu_sent = false;
                }
            }
        }

        this->get_previous_filter_ptr()->process_server_message(total_length,
            flags, chunk_data, decoded_header);
    }

private:
    bool verbose = false;

    bool server_clipboard_capabilities_pdu_sent = false;
};
