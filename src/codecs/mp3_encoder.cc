/*
 * mp3 encoder class source for mp3fs
 *
 * Copyright (C) 2013 K. Henriksson
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

#include "codecs/mp3_encoder.h"

#include <id3tag.h>
#include <lame/lame.h>

#include <cmath>
#include <cstdarg>
#include <cstdlib>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "buffer.h"
#include "logging.h"
#include "mp3fs.h"

/* Keep these items in static scope. */
namespace {

/* Value from LAME */
constexpr int kMaxVbrFrameSize = 2880;

constexpr int kMillisPerSec = 1000;

// Extra padding for buffers holding LAME output.
constexpr int kBufferSlop = 7200;

constexpr int kBitsPerByte = 8;

/* Callback functions for each type of lame message callback */
void lame_error(const char* fmt, va_list list) {
    log_with_level(ERROR, "LAME: ", fmt, list);
}
void lame_msg(const char* fmt, va_list list) {
    log_with_level(INFO, "LAME: ", fmt, list);
}
void lame_debug(const char* fmt, va_list list) {
    log_with_level(DEBUG, "LAME: ", fmt, list);
}

}  // namespace

/*
 * Create MP3 encoder. Do not set any parameters specific to a
 * particular file. Currently error handling is poor. If we run out
 * of memory, these routines will fail silently.
 */
Mp3Encoder::Mp3Encoder(Buffer* buffer) : buffer_(buffer) {
    id3tag_ = id3_tag_new();

    Log(DEBUG) << "LAME ready to initialize.";

    lame_encoder_ = lame_init();

    Mp3Encoder::set_text_tag(METATAG_ENCODER, PACKAGE_NAME);

    /* Set lame parameters. */
    if (params.vbr != 0) {
        lame_set_VBR(lame_encoder_, vbr_mt);
        lame_set_VBR_q(lame_encoder_, params.quality);
        lame_set_VBR_max_bitrate_kbps(lame_encoder_, params.bitrate);
        lame_set_bWriteVbrTag(lame_encoder_, 1);
    } else {
        lame_set_quality(lame_encoder_, params.quality);
        lame_set_brate(lame_encoder_, params.bitrate);
        lame_set_bWriteVbrTag(lame_encoder_, 0);
    }
    lame_set_errorf(lame_encoder_, &lame_error);
    lame_set_msgf(lame_encoder_, &lame_msg);
    lame_set_debugf(lame_encoder_, &lame_debug);
}

/*
 * Destroy private encode data. libid3tag asserts that id3tag is nonzero,
 * so we have to check ourselves to avoid this case.
 */
Mp3Encoder::~Mp3Encoder() {
    if (id3tag_ != nullptr) {
        id3_tag_delete(id3tag_);
    }
    lame_close(lame_encoder_);
}

/*
 * Set pcm stream parameters to be used by LAME encoder. This should be
 * called as soon as the information is available and must be called
 * before encode_pcm_data can be called.
 */
int Mp3Encoder::set_stream_params(uint64_t num_samples, int sample_rate,
                                  int channels) {
    lame_set_num_samples(lame_encoder_, num_samples);
    lame_set_in_samplerate(lame_encoder_, sample_rate);
    lame_set_num_channels(lame_encoder_, channels);

    Log(DEBUG) << "LAME partially initialized.";

    /* Initialise encoder */
    if (lame_init_params(lame_encoder_) == -1) {
        Log(ERROR) << "lame_init_params failed.";
        return -1;
    }

    Log(DEBUG) << "LAME initialized.";

    /*
     * Set the length in the ID3 tag, as this is the most convenient place
     * to do it.
     */
    std::ostringstream tempstr;
    tempstr << num_samples * kMillisPerSec / sample_rate;
    set_text_tag(METATAG_TRACKLENGTH, tempstr.str().c_str());

    return 0;
}

/*
 * Set an ID3 text tag (one whose name begins with "T") to have the
 * specified value. This can be called multiple times with the same key,
 * and the tag will receive multiple values for the tag, as allowed by the
 * standard. The tag is assumed to be encoded in UTF-8.
 */
void Mp3Encoder::set_text_tag(const int key, const char* value) {
    if (value == nullptr) {
        return;
    }

    auto it = kMetatagMap.find(key);

    if (it != kMetatagMap.end()) {
        struct id3_frame* frame = id3_tag_findframe(id3tag_, it->second, 0);
        if (frame == nullptr) {
            frame = id3_frame_new(it->second);
            id3_tag_attachframe(id3tag_, frame);

            id3_field_settextencoding(id3_frame_field(frame, 0),
                                      ID3_FIELD_TEXTENCODING_UTF_8);
        }

        id3_ucs4_t* ucs4 =
            id3_utf8_ucs4duplicate(reinterpret_cast<const id3_utf8_t*>(value));
        if (ucs4 != nullptr) {
            id3_field_addstring(id3_frame_field(frame, 1), ucs4);
            free(ucs4);
        }
        /* Special handling for track or disc numbers. */
    } else if (key == METATAG_TRACKNUMBER || key == METATAG_TRACKTOTAL ||
               key == METATAG_DISCNUMBER || key == METATAG_DISCTOTAL) {
        const char* tagname;
        if (key == METATAG_TRACKNUMBER || key == METATAG_TRACKTOTAL) {
            tagname = "TRCK";
        } else {
            tagname = "TPOS";
        }
        struct id3_frame* frame = id3_tag_findframe(id3tag_, tagname, 0);
        const id3_latin1_t* lat;
        id3_latin1_t* tofree = nullptr;
        if (frame != nullptr) {
            const id3_ucs4_t* pre =
                id3_field_getstrings(id3_frame_field(frame, 1), 0);
            tofree = id3_ucs4_latin1duplicate(pre);
            lat = tofree;
        } else {
            frame = id3_frame_new(tagname);
            id3_tag_attachframe(id3tag_, frame);
            id3_field_settextencoding(id3_frame_field(frame, 0),
                                      ID3_FIELD_TEXTENCODING_UTF_8);
            lat = reinterpret_cast<const id3_latin1_t*>("");
        }
        std::ostringstream tempstr;
        if (key == METATAG_TRACKNUMBER || key == METATAG_DISCNUMBER) {
            tempstr << value << lat;
        } else {
            tempstr << lat << "/" << value;
        }
        id3_ucs4_t* ucs4 = id3_latin1_ucs4duplicate(
            reinterpret_cast<const id3_latin1_t*>(tempstr.str().c_str()));
        if (ucs4 != nullptr) {
            id3_field_setstrings(id3_frame_field(frame, 1), 1, &ucs4);
            free(ucs4);
        }
        if (tofree != nullptr) {
            free(tofree);
        }
    }
}

/* Set an ID3 picture ("APIC") tag. */
void Mp3Encoder::set_picture_tag(const char* mime_type, int type,
                                 const char* description, const uint8_t* data,
                                 unsigned int data_length) {
    struct id3_frame* frame = id3_frame_new("APIC");
    id3_tag_attachframe(id3tag_, frame);

    id3_field_settextencoding(id3_frame_field(frame, 0),
                              ID3_FIELD_TEXTENCODING_UTF_8);
    id3_field_setlatin1(id3_frame_field(frame, 1),
                        reinterpret_cast<const id3_latin1_t*>(mime_type));
    id3_field_setint(id3_frame_field(frame, 2), type);
    id3_field_setbinarydata(id3_frame_field(frame, 4), data, data_length);

    id3_ucs4_t* ucs4 = id3_utf8_ucs4duplicate(
        reinterpret_cast<const id3_utf8_t*>(description));
    if (ucs4 != nullptr) {
        id3_field_setstring(id3_frame_field(frame, 3), ucs4);
        free(ucs4);
    }
}

/*
 * Set MP3 gain value in decibels. For MP3, there is no standard tag that can
 * be used, so the value is set directly as a gain in the encoder. The pow
 * formula comes from
 * http://replaygain.hydrogenaud.io/proposal/player_scale.html
 */
void Mp3Encoder::set_gain_db(const double dbgain) {
    Log(DEBUG) << "LAME setting gain to " << dbgain << ".";
    // NOLINTNEXTLINE(readability-magic-numbers)
    lame_set_scale(lame_encoder_, static_cast<float>(pow(10.0, dbgain / 20)));
}

/*
 * Render the ID3 tag into the referenced Buffer. This should be the first
 * thing to go into the Buffer. The ID3v1 tag will also be written 128
 * bytes from the calculated end of the buffer. It has a fixed size.
 */
int Mp3Encoder::render_tag(size_t file_size) {
    /*
     * Disable ID3 compression because it hardly saves space and some
     * players don't like it.
     * Also add 12 bytes of padding at the end, because again some players
     * are buggy.
     * Some players = iTunes
     */
    id3_tag_options(id3tag_, ID3_TAG_OPTION_COMPRESSION, 0);
    const int extra_padding = 12;
    id3_tag_setlength(id3tag_,
                      id3_tag_render(id3tag_, nullptr) + extra_padding);

    // write v2 tag
    id3size_ = id3_tag_render(id3tag_, nullptr);
    std::vector<uint8_t> tag24(id3size_);
    id3_tag_render(id3tag_, tag24.data());
    buffer_->write(tag24, true);

    // Write v1 tag at end of buffer.
    id3_tag_options(id3tag_, ID3_TAG_OPTION_ID3V1, ~0);
    std::vector<uint8_t> tag1(kId3v1TagLength);
    id3_tag_render(id3tag_, tag1.data());
    if (file_size == 0) {
        file_size = calculate_size();
    }
    buffer_->write_end(
        tag1, static_cast<std::ptrdiff_t>(file_size - kId3v1TagLength));

    return 0;
}

/*
 * Properly calculate final file size. This is the sum of the size of
 * ID3v2, ID3v1, and raw MP3 data. This is theoretically only approximate
 * but in practice gives excellent answers, usually exactly correct.
 * Cast to 64-bit int to avoid overflow.
 *
 * MP3 data is organized in frames, and each frame contains 1152 samples. So
 *   samples = frames * 1152
 *   length (sec) = samples / samplerate
 *   bytes = bits / 8 = length (sec) * bitrate / 8
 *         = frames * 1152 * bitrate / 8 / samplerate
 *         = frames * 144 * bitrate / samplerate
 * Note that the true bitrate is 1000 times the value stored in params.bitrate,
 * so our conversion factor is actually 144000.
 */
size_t Mp3Encoder::calculate_size() const {
    const int conversion_factor = 144000;
    if (params.vbr != 0) {
        return id3size_ + kId3v1TagLength + kMaxVbrFrameSize +
               static_cast<uint64_t>(lame_get_totalframes(lame_encoder_)) *
                   conversion_factor * params.bitrate /
                   lame_get_in_samplerate(lame_encoder_);
    }
    return id3size_ + kId3v1TagLength +
           static_cast<uint64_t>(lame_get_totalframes(lame_encoder_)) *
               conversion_factor * params.bitrate /
               lame_get_out_samplerate(lame_encoder_);
}

/*
 * Encode the given PCM data into the given Buffer. This function must not
 * be called before the encoder has finished initialization with
 * set_stream_params(). It should be called after the ID3 tag has been
 * rendered into the Buffer with render_tag(). The data is given as a
 * multidimensional array of channel data in the format previously
 * specified, as 32-bit right-aligned signed integers. Right-alignment
 * means that the range of values from each sample should be
 * -2**(sample_size-1) to 2**(sample_size-1)-1. This is not coincidentally
 * the format used by the FLAC library.
 */
int Mp3Encoder::encode_pcm_data(const int32_t* const data[],
                                unsigned int numsamples,
                                unsigned int sample_size) {
    /*
     * We need to properly resample input data to a format LAME wants. LAME
     * requires samples in a C89 sized type, left aligned (i.e. scaled to
     * the maximum value of the type) and we cannot be sure for example how
     * large an int is. We require it be at least 32 bits on all platforms
     * that will run mp3fs, and rescale to the appropriate size. Cast
     * first to avoid integer overflow.
     */
    std::vector<int> lbuf(numsamples), rbuf(numsamples);
    for (unsigned int i = 0; i < numsamples; ++i) {
        lbuf[i] = data[0][i] << (sizeof(int) * kBitsPerByte - sample_size);
        /* ignore rbuf for mono data */
        if (lame_get_num_channels(lame_encoder_) > 1) {
            rbuf[i] = data[1][i] << (sizeof(int) * kBitsPerByte - sample_size);
        }
    }

    // Buffer size formula recommended by LAME docs: 1.25 * samples + 7200
    std::vector<uint8_t> vbuffer(5 * numsamples / 4 + kBufferSlop);  // NOLINT

    const int len = lame_encode_buffer_int(
        lame_encoder_, lbuf.data(), rbuf.data(), static_cast<int>(numsamples),
        vbuffer.data(), static_cast<int>(vbuffer.size()));
    if (len < 0) {
        return -1;
    }
    vbuffer.resize(len);

    buffer_->write(vbuffer, false);

    return 0;
}

/*
 * Encode any remaining PCM data in LAME internal buffers to the given
 * Buffer. This should be called after all input data has already been
 * passed to encode_pcm_data().
 */
int Mp3Encoder::encode_finish() {
    std::vector<uint8_t> vbuffer(kBufferSlop);

    const int len = lame_encode_flush(lame_encoder_, vbuffer.data(),
                                      static_cast<int>(vbuffer.size()));
    if (len < 0) {
        return -1;
    }
    vbuffer.resize(len);

    buffer_->write(vbuffer, params.statcachesize > 0);
    if (params.statcachesize > 0) {
        buffer_->truncate();
    } else {
        buffer_->extend();
    }

    /*
     * Write the VBR tag data at id3size bytes after the beginning. lame
     * already put dummy bytes here when lame_init_params() was called.
     */
    if (params.vbr != 0) {
        std::vector<uint8_t> tail(kMaxVbrFrameSize);
        const size_t vbr_tag_size = lame_get_lametag_frame(
            lame_encoder_, tail.data(), kMaxVbrFrameSize);
        if (vbr_tag_size > kMaxVbrFrameSize) {
            return -1;
        }
        buffer_->write_to(tail, static_cast<std::ptrdiff_t>(id3size_));
    }

    return len;
}

/*
 * This map contains the association from the standard values in the enum in
 * coders.h to ID3 values.
 */
const Mp3Encoder::meta_map_t Mp3Encoder::kMetatagMap = {
    {METATAG_TITLE, "TIT2"},     {METATAG_ARTIST, "TPE1"},
    {METATAG_ALBUM, "TALB"},     {METATAG_GENRE, "TCON"},
    {METATAG_DATE, "TDRC"},      {METATAG_COMPOSER, "TCOM"},
    {METATAG_PERFORMER, "TOPE"}, {METATAG_COPYRIGHT, "TCOP"},
    {METATAG_ENCODEDBY, "TENC"}, {METATAG_ORGANIZATION, "TPUB"},
    {METATAG_CONDUCTOR, "TPE3"}, {METATAG_ALBUMARTIST, "TPE2"},
    {METATAG_ENCODER, "TSSE"},   {METATAG_TRACKLENGTH, "TLEN"},
};
