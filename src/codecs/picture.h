/*
 * FLAC-format PICTURE handler class header for mp3fs
 *
 * Copyright (C) 2017 K. Henriksson
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef MP3FS_CODECS_PICTURE_H_
#define MP3FS_CODECS_PICTURE_H_

#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

class Picture {
 public:
    explicit Picture(std::vector<char> data) : data_(std::move(data)) {}

    bool decode();

    int get_type() const { return static_cast<int>(type_); }
    const char* get_mime_type() const { return mime_type_.c_str(); }
    const char* get_description() const { return description_.c_str(); }
    int get_data_length() const {
        return static_cast<int>(picture_data_.size());
    }
    const uint8_t* get_data() const { return picture_data_.data(); }

 private:
    bool consume_decode_uint32(uint32_t* out);
    bool consume_decode_string(std::string* out);

    bool consume_no_decode(size_t size) {
        data_off_ += size;
        return true;
    }

    std::vector<char> data_;
    size_t data_off_ = 0;

    uint32_t type_ = 0;
    std::string mime_type_, description_;
    std::vector<uint8_t> picture_data_;
};

#endif  // MP3FS_CODECS_PICTURE_H_
