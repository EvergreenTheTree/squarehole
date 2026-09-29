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

#ifndef SQUAREHOLE_AUDIO_H
#define SQUAREHOLE_AUDIO_H

#include "glib.h"
#include <libavcodec/avcodec.h>
#include <libswresample/swresample.h>

typedef struct
{
  const AVCodec *codec;
  AVCodecContext *codec_context;
  AVPacket *pkt;
  AVFrame *frame;
  SwrContext *swr;
} TranscoderContext;

typedef struct
{
  TranscoderContext *encoder;
  TranscoderContext *decoder;
  gboolean configured;
} Transcoder;

/**
 * Allocate new Transcoder object. Use transcoder_configure to set a codec
 * before using the rest of the transcoder functions
 *
 * @retval NULL     Error during any part of allocation or initialization
 * @retval != NULL  Pointer to the new Transcoder
 */
Transcoder *transcoder_alloc (void);

/**
 * Configures the transcoder to use the given codec to decode. Sets up
 * underlying ffmpeg objects for use in the rest of the transcoder functions
 * transcoding from mono audio of the type specified by raw_codec into floating
 * point 32 bit audio samples. Can be called more than once.
 *
 * @param raw_codec  ffmpeg audio codec representing the type of audio you want
 *                   to treat the incoming bytes as. Must be a raw pcm_ codec
 *                   for a format that contains no header data (no pcm_bluray,
 *                   pcm_dvd, pcm_s24daud or pcm_vidc).
 * @return 0 on success or ffmpeg AVERROR error code on failure. These will
 *         either be AVERROR(ENOMEM) for failures to allocate objects or
 *         errors returned by swr_init / avcodec_open2.
 */
gint transcoder_configure (Transcoder *transcoder, enum AVCodecID raw_codec);

/**
 * Frees all context and underlying ffmpeg objects associated with transcoder.
 * Sets the passed pointer to NULL.
 *
 * @param transcoder  the transcoder object to free
 */
void transcoder_free (Transcoder **transcoder);

/**
 * Treats arbitrary bytes as audio samples (the size and format of which are
 * determined by the transcoder configuration) and converts them into mono
 * 32-bit floating point audio samples.
 *
 * @param transcoder    allocated and configured transcoder object
 * @param input         bytes to treat as audio samples to convert
 * @param input_samples how large the input is in samples; how many bytes that
 *                      is depends on the transcoder configuration
 * @param output        buffer to store resulting 32-bit fp samples to; should
 *                      be large enough to store input_samples floating point
 *                      samples.
 * @return 0 on success or ffmpeg AVERROR error code on failure
 */
gint transcoder_decode (Transcoder *transcoder, guint8 *input,
                        gint input_samples, gfloat *output);

/**
 * Takes mono 32-bit floating point audio samples and converts them back into
 * arbitrary bytes (the size and format of which are determined by the
 * transcoder configuration).
 *
 * @param transcoder     allocated and configured transcoder object
 * @param input          32-bit fp samples to convert
 * @param input_samples  how large the input is in samples
 * @param output         buffer to store resulting bytes to. This should be
 *                       large enough to store bytes equal to input_samples
 *                       times the number of output channels (1) times the
 *                       number of bytes per output sample.
 * @return 0 on success or ffmpeg AVERROR error code on failure
 */
gint transcoder_encode (Transcoder *transcoder, gfloat *input,
                        gint input_samples, guint8 *output);

#endif /* SQUAREHOLE_AUDIO_H */
