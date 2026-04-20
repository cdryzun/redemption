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
Copyright (C) Wallix 2010-2018
Author(s): Jonathan Poelen
*/

#pragma once

#include "mod/mod_api.hpp"
#include "mod/vnc/vnc_verbose.hpp"
#include "configs/autogen/enums.hpp"

#include <memory>
#include <string_view>

class ClientExecute;
class FrontAPI;
class Transport;
class ModTlsParams;
class SessionLogApi;
class KeyLayout;
class Random;
class EventContainer;

namespace gdi
{
    class GraphicApi;
}

namespace kbdtypes
{
    enum class KeyLocks : uint8_t;
}

std::unique_ptr<mod_api> new_mod_vnc(
    Transport & t
  , Random & random
  , gdi::GraphicApi & gd
  , EventContainer & events
  , const char * username
  , const char * password
  , FrontAPI & front
  , uint16_t front_width
  , uint16_t front_height
  , bool clipboard_up
  , bool clipboard_down
  , const char * encodings
  , VncClipboardEncoding clipboard_server_encoding_type
  , VncBogusClipboardInfiniteLoopStrategy bogus_clipboard_infinite_loop
  , KeyLayout const& layout
  , kbdtypes::KeyLocks locks
  , bool server_is_macos
  , bool send_alt_ksym
  , bool cursor_pseudo_encoding_supported
  , ClientExecute* rail_client_execute
  , VNCVerbose verbose
  , SessionLogApi& session_log
  , ModTlsParams const& tls_params
  , std::string_view force_authentication_method
);
