/*
SPDX-FileCopyrightText: 2026 Wallix Proxies Team
SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "core/file_validator/file_validator_targets.hpp"

class FileValidatorService;

struct FileValidatorParams
{
    FileValidatorService * file_validator_service;

    FileValidatorTargets targets;

    bool log_if_accepted;

    bool block_invalid_file_upload;
    bool block_invalid_file_download;

    uint64_t max_blocked_file_size_rejected;
    chars_view tmp_dir;

    explicit operator bool () const noexcept
    {
        return file_validator_service;
    }
};
