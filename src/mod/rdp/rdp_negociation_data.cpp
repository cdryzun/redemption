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

  rdp module main
*/

#include <cstring>
#include "utils/log.hpp"
#include "utils/strutils.hpp"
#include "mod/rdp/rdp_negociation_data.hpp"

RdpLogonInfo::RdpLogonInfo(RdpHostname hostname, bool hide_client_name,
                           chars_view target_user, bool split_domain) noexcept :
    hide_client_name(hide_client_name)
{
    if (hide_client_name) {
        this->_hostname.delayed_build([](auto buffer) {
            auto array = buffer.chars_with_null_terminated();
            ::gethostname(array.data(), array.size());
            array.back() = '\0';
            char* separator = strchr(array.data(), '.');
            if (!separator) {
                return buffer.compute_strlen();
            }
            return buffer.set_end_string_ptr(separator);
        });
    }
    else {
        this->_hostname = hostname.utf8_fixed_maybe_invalid().zstr();
    }

    chars_view username = target_user;
    chars_view domain {};

    const char * separator = strchr(target_user, '\\');
    const char * separator_a = strchr(target_user, '@');

    if (separator && !separator_a)
    {
        // Legacy username
        // Split only if there's no @, otherwise not a legacy username
        domain = target_user.before(separator);
        username = target_user.after(separator);
    }
    else if (split_domain)
    {
        // Old behavior
        if (separator)
        {
            domain = target_user.before(separator);
            username = target_user.after(separator);
        }
        else if (separator_a)
        {
            domain = target_user.after(separator_a);
            username = target_user.before(separator_a);
            LOG(LOG_INFO, "mod_rdp: username_len=%zu", username.size());
        }
    }

    this->_username.assign(username.begin(), username.end());
    this->_domain.assign(domain.begin(), domain.end());

    LOG(LOG_INFO, "Remote RDP Server domain=\"%s\" login=\"%s\" host=\"%s\"",
        this->_domain, this->_username, this->_hostname);
}
