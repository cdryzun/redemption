/*
*   This program is free software; you can redistribute it and/or modify
*   it under the terms of the GNU General Public License as published by
*   the Free Software Foundation; either version 2 of the License, or
*   (at your option) any later version.
*
*   This program is distributed in the hope that it will be useful,
*   but WITHOUT ANY WARRANTY; without even the implied warranty of
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*   GNU General Public License for more details.
*
*   You should have received a copy of the GNU General Public License
*   along with this program; if not, write to the Free Software
*   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*
*   Product name: redemption, a FLOSS RDP proxy
*   Copyright (C) Wallix 2013-2017
*   Author(s): Christophe Grosjean, Jonatan Poelen, Raphael Zhou, Meng Tan
*/

#include "transport/out_filename_sequence_transport.hpp"
#include "utils/strutils.hpp"
#include "utils/log.hpp"

#include <cstring>


OutFilenameSequenceTransport::FilenameGenerator::FilenameGenerator(
    const char * const prefix,
    const char * const filename,
    const char * const extension)
: last_num(-1u)
{
    int len = snprintf(this->filename_gen, sizeof(this->filename_gen), "%s%s-", prefix, filename);
    if (len <= 0
     || len >= int(sizeof(this->filename_gen) - 1)
     || !utils::strbcpy(this->extension, extension)
    ) {
        LOG(LOG_ERR, "Filename too long");
        throw Error(ERR_TRANSPORT);
    }

    this->filename_suffix_pos = std::size_t(len);
}

const char * OutFilenameSequenceTransport::FilenameGenerator::get(unsigned count) const
{
    if (count == this->last_num) {
        return this->filename_gen;
    }

    snprintf( this->filename_gen + this->filename_suffix_pos
            , sizeof(this->filename_gen) - this->filename_suffix_pos
            , "%06u%s", count, this->extension);
    return this->filename_gen;
}


OutFilenameSequenceTransport::OutFilenameSequenceTransport(
    const char * const prefix,
    const char * const filename,
    const char * const extension,
    FilePermissions file_permissions,
    FileTransport::ErrorNotifier notify_error)
: filegen_(prefix, filename, extension)
, buf_(invalid_fd(), std::move(notify_error))
, file_permissions_(file_permissions)
{
}

char const* OutFilenameSequenceTransport::seqgen(unsigned i) const noexcept
{
    return this->filegen_.get(i);
}

bool OutFilenameSequenceTransport::next()
{
    if (!this->status) {
        LOG(LOG_ERR, "OutFilenameSequenceTransport::next: Invalid status!");
        throw Error(ERR_TRANSPORT_NO_MORE_DATA);
    }

    if (this->do_next()) {
        this->status = false;
        LOG(LOG_ERR, "OutFilenameSequenceTransport::next: Create next file failed!");
        throw Error(ERR_TRANSPORT_WRITE_FAILED, errno);
    }

    ++this->seqno_;
    return true;
}

bool OutFilenameSequenceTransport::disconnect()
{
    return this->do_next();
}

OutFilenameSequenceTransport::~OutFilenameSequenceTransport()
{
    this->do_next();
}

void OutFilenameSequenceTransport::do_send(const uint8_t * data, size_t len)
{
    if (REDEMPTION_UNLIKELY(!this->buf_.is_open())) {
        this->open_filename();
    }
    this->buf_.send(data, len);
}

/// \return 0 if success
int OutFilenameSequenceTransport::do_next()
{
    if (this->buf_.is_open()) {
        this->buf_.close();
        ++this->num_file_;
        return 0;
    }

    return 1;
}

void OutFilenameSequenceTransport::open_filename()
{
    const char * filename = this->filegen_.get(this->num_file_);
    const auto mode = file_permissions_.permissions_as_uint();
    const int fd = ::open(filename, O_WRONLY | O_CREAT | O_TRUNC, mode);
    if (fd < 0) {
        throw Error(ERR_TRANSPORT_OPEN_FAILED, errno);
    }
    this->buf_.open(unique_fd{fd});
}
