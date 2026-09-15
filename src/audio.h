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

AudioTranscoder *
alloc_transcoder (void);

void
free_transcoder (AudioTranscoder *transcoder);

// Takes arbitrary bytes as input and converts it into stereo 32-bit floating
// point audio.
void
transcoder_decode (AudioTranscoder *transcoder, guint8 *input, gint input_size,
                   guint8 *output);

// Takes 32-bit floating point audio data and converts it back into the format
// from whence it came (prior to transcoder_decode).
void
transcoder_encode (AudioTranscoder *transcoder, guint8 *input, gint input_size,
                   guint8 *output);

void
transcoder_reconfigure (AudioTranscoder *transcoder);

#endif /* SQUAREHOLE_AUDIO_H */
