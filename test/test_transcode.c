/* squarehole -- GEGL operation that fits a circle (image data) into a square
 * hole (audio effects) Copyright (C) 2026 Evergreen
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "transcode.h"
#include <criterion/criterion.h>
#include <criterion/new/assert.h>
#include <stdio.h>

void
ffmpeg_log (void *ptr, int level, const char *fmt, va_list vargs)
{
  vfprintf (stderr, fmt, vargs);
}

void
setup (void)
{
  av_log_set_level (AV_LOG_ERROR);
  av_log_set_callback (ffmpeg_log);
}

void
teardown (void)
{
}

Test (transcode, decode_matches_ffmpeg_cli)
{
  AudioTranscoder *transcoder = alloc_transcoder ();

  guint8 in_samples[8] = { 0, 1, 2, 3, 4, 5, 6, 7 };
  // generated with:
  // printf '\x00\x01\x02\x03\x04\x05\x06\x07' |
  // ffmpeg -loglevel quiet -f mulaw -ac 1 -i - -f f32le -ac 1 pipe:1 | xxd -i
  guint8 expected_out_samples[32]
      = { 0x00, 0xf8, 0x7a, 0xbf, 0x00, 0xf8, 0x72, 0xbf, 0x00, 0xf8, 0x6a,
          0xbf, 0x00, 0xf8, 0x62, 0xbf, 0x00, 0xf8, 0x5a, 0xbf, 0x00, 0xf8,
          0x52, 0xbf, 0x00, 0xf8, 0x4a, 0xbf, 0x00, 0xf8, 0x42, 0xbf };

  gfloat out_samples[9] = { 0 };
  transcoder_decode (transcoder, in_samples, 8, out_samples);

  cr_expect (eq (flt, out_samples[8], 0.0),
             "Buffer should not be overrun by transcoder_decode");
  cr_expect (eq (flt[8], out_samples, (gfloat *)expected_out_samples),
             "transcoder_decode should match ffmpeg CLI");

  free_transcoder (transcoder);
}

Test (transcode, encode_works)
{
  AudioTranscoder *transcoder = alloc_transcoder ();

  // generated with
  // printf '\x00\x01\x02\x03\x04\x05\x06\x07' |
  // ffmpeg -loglevel quiet -f mulaw -ac 1 -i - -f f32le -ac 1 pipe:1 | xxd -i
  guint8 in_samples[32]
      = { 0x00, 0xf8, 0x7a, 0xbf, 0x00, 0xf8, 0x72, 0xbf, 0x00, 0xf8, 0x6a,
          0xbf, 0x00, 0xf8, 0x62, 0xbf, 0x00, 0xf8, 0x5a, 0xbf, 0x00, 0xf8,
          0x52, 0xbf, 0x00, 0xf8, 0x4a, 0xbf, 0x00, 0xf8, 0x42, 0xbf };
  guint8 out_samples_expected[8] = { 0, 1, 2, 3, 4, 5, 6, 7 };

  guint8 out_samples[9] = { 0 };
  transcoder_encode (transcoder, (gfloat *)in_samples, 8, out_samples);

  cr_expect (eq (u8, out_samples[8], 0),
             "Buffer should not be overrun by transcoder_encode");
  cr_expect (eq (u8[8], out_samples, out_samples_expected),
             "transcoder_encode should match ffmpeg CLI");

  free_transcoder (transcoder);
}
