/*
SPDX-FileCopyrightText: 2026 Wallix Proxies Team
SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "core/file_validator/file_validator_params.hpp"
#include "core/file_storage_params.hpp"

#include <chrono>


struct ClipboardVirtualChannelParams
{
    struct CliprdrParams
    {
        bool download_authorized;
        bool upload_authorized;
        bool file_authorized;
        bool log_only_relevant_activities;
        bool log_text;
    };

    struct ValidatorParams
    {
        struct TextValidator
        {
            bool enable_text_upload;
            bool enable_text_download;

            bool block_invalid_text_upload;
            bool block_invalid_text_download;
        };

        FileValidatorParams file;
        TextValidator text;
        std::chrono::seconds osd_delay;
    };

    CliprdrParams cliprdr_params;
    ValidatorParams validator_params;
    FileStorageParams file_storage_params;
};
