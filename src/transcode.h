/* squarehole -- GEGL operation that fits a circle (image data) into a square hole (audio effects)
 * Copyright (C) 2026 Evergreen
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

#ifndef SQUAREHOLE_AUDIO_H
#define SQUAREHOLE_AUDIO_H

#include <libavcodec/avcodec.h>
#include <libswresample/swresample.h>
#include "glib.h"

typedef struct
{
  const AVCodec *codec;
  AVCodecContext *codec_context;
  AVPacket *pkt;
  AVFrame *frame;
  SwrContext *swr;
  gboolean configured;
} AudioTranscoderContext;

typedef struct
{
  AudioTranscoderContext *encoder;
  AudioTranscoderContext *decoder;
} AudioTranscoder;

/**
 * Allocate new AudioTranscoder. Allocates context and sets up underlying
 * ffmpeg objects for use in the rest of the transcoder functions.
 *
 * @retval NULL     Error during any part of allocation or initialization
 * @retval != NULL  Pointer to the new AudioTranscoder
 */
AudioTranscoder *
alloc_transcoder (void);

/**
 *
 */
void
transcoder_configure (AudioTranscoder *transcoder);

/**
 * Frees all context and underlying ffmpeg objects associated with transcoder.
 *
 * @param transcoder the transcoder object to free
 */
void
free_transcoder (AudioTranscoder *transcoder);

/**
 * Treats arbitrary bytes as audio samples (the size and format of which are
 * determined by the transcoder configuration) and converts them into stereo
 * 32-bit floating point audio samples. (TODO interleaved or planar? Airwindows takes planar)
 *
 * @param transcoder    allocated and configured transcoder object
 * @param input         bytes to treat as audio samples to convert
 * @param input_samples how large the input is in samples; how many bytes that
 *                      is depends on the transcoder configuration
 * @param output        buffer to store resulting 32-bit fp samples to; should
 *                      be large enough to store input_samples * 2 (channels,
 *                      stereo) floating point samples. (TODO interleaved or planar?)
 * @return 0 on success or ffmpeg AVERROR error code on failure
 */
gint
transcoder_decode (AudioTranscoder *transcoder, guint8 *input, gint input_samples,
                   gfloat *output);

/**
 * Takes stereo (TODO interleaved or planar?) 32-bit floating point audio samples
 * and converts them back into arbitrary bytes (the size and format of which are
 * determined by the transcoder configuration).
 *
 * @param transcoder    allocated and configured transcoder object
 * @param input         32-bit fp samples to convert
 * @param input_samples how large the input is in samples
 * @param output        buffer to store resulting bytes to. This should be
 *                      large enough to store bytes equal to input_samples
 *                      times the number of output channels times the number of
 *                      bytes per output sample. (TODO: API for this?)
 * @return 0 on success or ffmpeg AVERROR error code on failure
 */
gint
transcoder_encode (AudioTranscoder *transcoder, gfloat *input, gint input_samples,
                   guint8 *output);

#endif /* SQUAREHOLE_AUDIO_H */
