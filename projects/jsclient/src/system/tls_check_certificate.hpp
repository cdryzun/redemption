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
   Copyright (C) Wallix 2013
   Author(s): Christophe Grosjean, Raphael Zhou, Meng Tan

   Transport layer abstraction, socket implementation with TLS support
*/

#pragma once

#include "configs/autogen/enums.hpp"
#include "utils/basic_function.hpp"
#include "utils/sugar/array_view.hpp"
#include "core/certificate_enums.hpp"

#include <string_view>

class X509;

[[nodiscard]] inline bool tls_check_certificate(
    X509& /*x509*/,
    bool /*server_cert_store*/,
    ServerCertCheck /*server_cert_check*/,
    BasicFunction<void(CertificateStatus status, std::string_view error_msg)> /*certificate_checker*/,
    const char* /*certif_path*/,
    const char* /*protocol*/,
    const char* /*ip_address*/,
    int /*port*/)
{
  return false;
}
