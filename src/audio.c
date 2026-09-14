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

// TODO: error checks after every allocation and ffmpeg function

#include "audio.h"
#include "glib.h"

#include <libavcodec/packet.h>
#include <libavutil/avutil.h>
#include <libavutil/channel_layout.h>
#include <libavutil/frame.h>
#include <libavutil/mem.h>
#include <libavutil/opt.h>
#include <libavcodec/avcodec.h>
#include <libswresample/swresample.h>
#include <string.h>

AudioTranscoder *
alloc_transcoder (void)
{
  AudioTranscoder *transcoder = g_new(AudioTranscoder, 1);

  AudioTranscoderContext *encoder = g_new(AudioTranscoderContext, 1);
  transcoder->encoder = encoder;
  AudioTranscoderContext *decoder = g_new(AudioTranscoderContext, 1);
  transcoder->decoder = decoder;

  decoder->configured = TRUE;
  // TODO make this configurable
  const AVCodec *decoder_codec = avcodec_find_decoder(AV_CODEC_ID_PCM_MULAW);
  decoder->codec = decoder_codec;
  decoder->codec_context = avcodec_alloc_context3(decoder_codec);
  decoder->codec_context->sample_rate = 44100;
  decoder->codec_context->ch_layout = (AVChannelLayout)AV_CHANNEL_LAYOUT_MONO;
  if (avcodec_open2(decoder->codec_context, decoder_codec, NULL) < 0)
    {
      return NULL;
    }
  decoder->pkt = av_packet_alloc();
  decoder->frame = av_frame_alloc();

  encoder->configured = TRUE;
  // TODO make this configurable
  const AVCodec *encoder_codec = avcodec_find_encoder(AV_CODEC_ID_PCM_MULAW);
  encoder->codec = encoder_codec;
  encoder->codec_context = avcodec_alloc_context3(encoder_codec);
  encoder->codec_context->sample_rate = 44100;
  encoder->codec_context->ch_layout = (AVChannelLayout)AV_CHANNEL_LAYOUT_MONO;
  if (avcodec_open2(decoder->codec_context, encoder_codec, NULL) < 0)
    {
      return NULL;
    }
  encoder->pkt = av_packet_alloc();
  encoder->frame = av_frame_alloc();

  decoder->swr = swr_alloc();
  av_opt_set_chlayout(decoder->swr, "in_chlayout", &(AVChannelLayout)AV_CHANNEL_LAYOUT_MONO, 0);
  av_opt_set_chlayout(decoder->swr, "out_chlayout", &(AVChannelLayout)AV_CHANNEL_LAYOUT_STEREO, 0);
  av_opt_set_int(decoder->swr, "in_sample_rate", 44100, 0);
  av_opt_set_int(decoder->swr, "out_sample_rate", 44100, 0);
  av_opt_set_sample_fmt(decoder->swr, "in_sample_fmt", AV_SAMPLE_FMT_S16, 0);
  av_opt_set_sample_fmt(decoder->swr, "out_sample_fmt", AV_SAMPLE_FMT_FLTP, 0);
  // swr_init(decoder->swr);
  encoder->swr = swr_alloc();
  av_opt_set_chlayout(encoder->swr, "in_chlayout", &(AVChannelLayout)AV_CHANNEL_LAYOUT_STEREO, 0);
  av_opt_set_chlayout(encoder->swr, "out_chlayout", &(AVChannelLayout)AV_CHANNEL_LAYOUT_MONO, 0);
  av_opt_set_int(encoder->swr, "in_sample_rate", 44100, 0);
  av_opt_set_int(encoder->swr, "out_sample_rate", 44100, 0);
  av_opt_set_sample_fmt(encoder->swr, "in_sample_fmt", AV_SAMPLE_FMT_FLTP, 0);
  av_opt_set_sample_fmt(encoder->swr, "out_sample_fmt", AV_SAMPLE_FMT_S16, 0);
  swr_init(encoder->swr);

  return transcoder;
}

void
free_transcoder (AudioTranscoder *transcoder)
{
  swr_free(&transcoder->decoder->swr);
  swr_free(&transcoder->encoder->swr);
  avcodec_free_context(&transcoder->decoder->codec_context);
  avcodec_free_context(&transcoder->encoder->codec_context);
  g_free(transcoder->decoder);
  g_free(transcoder->encoder);
  g_free(transcoder);
}

void
transcoder_decode (AudioTranscoder *transcoder, guint8 *input, gint input_size,
                   guint8 *output)
{
  AudioTranscoderContext *decoder = transcoder->decoder;
  guint8 *output_ptr = output;

  enum AVSampleFormat sample_format;
  av_opt_get_sample_fmt(decoder->swr, "out_sample_fmt", 0, &sample_format);

  av_new_packet(decoder->pkt, input_size);
  memcpy(decoder->pkt->data, input, input_size);

  int ret = avcodec_send_packet(decoder->codec_context, decoder->pkt);

  while (ret >= 0) {
    AVFrame *frame = decoder->frame;
    ret = avcodec_receive_frame(decoder->codec_context, frame);
    if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
      ret = -1;
      break;
    }

    swr_convert(decoder->swr, &output_ptr, frame->nb_samples,
                (const guint8 *const *) frame->data, frame->nb_samples);
    output_ptr += frame->nb_samples;
  }

  av_packet_unref(decoder->pkt);
}

void
transcoder_encode (AudioTranscoder *transcoder, guint8 *input, gint input_size,
                   guint8 *output)
{
  // Set frame parameters and allocate buffer (this probably can be done in reconfigure?)
  // Loop until all of input is used up
  //   Resample from input to frames's buffer, keeping track of how much of input has been used
  //   Send frame to encoder
  //   Loop:  Receive packets of encoded samples and copy to output
  //
}
