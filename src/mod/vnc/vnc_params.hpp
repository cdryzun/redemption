/*
SPDX-FileCopyrightText: 2026 Wallix Proxies Team
SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "core/file_validator/file_validator_params.hpp"
#include "core/file_storage_params.hpp"
#include "configs/autogen/enums.hpp"
#include "utils/basic_function.hpp"


class FileValidatorService;
class FdxCapture;


struct ModVncParams
{
    struct CutTextParams
    {
        bool enable_upload;
        bool enable_download;
        VncClipboardEncoding server_encoding;
        VncBogusClipboardInfiniteLoopStrategy bogus_infinite_loop_strategy;
    };

    struct FileTransferParams
    {
        bool enable_upload;
        bool enable_download;
        uint32_t max_item_in_gui;
        uint32_t max_file_list;
        uint64_t max_file_size;
    };

    struct FileValidatorAndStorage
    {
        FileValidatorParams file_validator;
        FileStorageParams file_storage;
    };

    using GetFileValidatorAndStorage = BasicFunction<FileValidatorAndStorage()>;

    CutTextParams cut_text;
    FileTransferParams file_transfer;
    GetFileValidatorAndStorage get_file_validator_and_storage;
};
