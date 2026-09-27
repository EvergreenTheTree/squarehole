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
#include "glib.h"

#include <libavcodec/avcodec.h>
#include <libavcodec/packet.h>
#include <libavutil/avutil.h>
#include <libavutil/channel_layout.h>
#include <libavutil/frame.h>
#include <libavutil/mem.h>
#include <libavutil/opt.h>
#include <libavutil/samplefmt.h>
#include <libswresample/swresample.h>
#include <string.h>

AudioTranscoder *
alloc_transcoder (void)
{
  AudioTranscoder *transcoder = g_new (AudioTranscoder, 1);
  gint ret;

  AudioTranscoderContext *encoder = g_new (AudioTranscoderContext, 1);
  if (encoder == NULL)
    return NULL;
  transcoder->encoder = encoder;
  AudioTranscoderContext *decoder = g_new (AudioTranscoderContext, 1);
  if (decoder == NULL)
    return NULL;
  transcoder->decoder = decoder;

  decoder->configured = TRUE;
  // TODO make this configurable
  const AVCodec *decoder_codec = avcodec_find_decoder (AV_CODEC_ID_PCM_MULAW);
  decoder->codec = decoder_codec;
  decoder->codec_context = avcodec_alloc_context3 (decoder_codec);
  if (decoder->codec_context == NULL)
    return NULL;
  decoder->codec_context->sample_rate = 44100;
  decoder->codec_context->ch_layout = (AVChannelLayout)AV_CHANNEL_LAYOUT_MONO;
  ret = avcodec_open2 (decoder->codec_context, decoder_codec, NULL);
  if (ret < 0)
    return NULL;

  encoder->configured = TRUE;
  // TODO make this configurable
  const AVCodec *encoder_codec = avcodec_find_encoder (AV_CODEC_ID_PCM_MULAW);
  encoder->codec = encoder_codec;
  encoder->codec_context = avcodec_alloc_context3 (encoder_codec);
  if (encoder->codec_context == NULL)
    return NULL;
  encoder->codec_context->sample_rate = 44100;
  encoder->codec_context->ch_layout = (AVChannelLayout)AV_CHANNEL_LAYOUT_MONO;
  encoder->codec_context->sample_fmt = AV_SAMPLE_FMT_S16;
  ret = avcodec_open2 (encoder->codec_context, encoder_codec, NULL);
  if (ret < 0)
    return NULL;

  decoder->swr = swr_alloc ();
  if (decoder->swr == NULL)
    return NULL;
  av_opt_set_chlayout (decoder->swr, "in_chlayout",
                       &(AVChannelLayout)AV_CHANNEL_LAYOUT_MONO, 0);
  av_opt_set_chlayout (decoder->swr, "out_chlayout",
                       &(AVChannelLayout)AV_CHANNEL_LAYOUT_MONO, 0);
  av_opt_set_int (decoder->swr, "in_sample_rate", 44100, 0);
  av_opt_set_int (decoder->swr, "out_sample_rate", 44100, 0);
  av_opt_set_sample_fmt (decoder->swr, "in_sample_fmt", AV_SAMPLE_FMT_S16, 0);
  av_opt_set_sample_fmt (decoder->swr, "out_sample_fmt", AV_SAMPLE_FMT_FLT, 0);
  ret = swr_init (decoder->swr);
  if (ret < 0)
    return NULL;

  encoder->swr = swr_alloc ();
  if (encoder->swr == NULL)
    return NULL;
  av_opt_set_chlayout (encoder->swr, "in_chlayout",
                       &(AVChannelLayout)AV_CHANNEL_LAYOUT_MONO, 0);
  av_opt_set_chlayout (encoder->swr, "out_chlayout",
                       &(AVChannelLayout)AV_CHANNEL_LAYOUT_MONO, 0);
  av_opt_set_int (encoder->swr, "in_sample_rate", 44100, 0);
  av_opt_set_int (encoder->swr, "out_sample_rate", 44100, 0);
  av_opt_set_sample_fmt (encoder->swr, "in_sample_fmt", AV_SAMPLE_FMT_FLT, 0);
  av_opt_set_sample_fmt (encoder->swr, "out_sample_fmt", AV_SAMPLE_FMT_S16, 0);
  ret = swr_init (encoder->swr);
  if (ret < 0)
    return NULL;

  return transcoder;
}

void
free_transcoder (AudioTranscoder *transcoder)
{
  swr_free (&transcoder->decoder->swr);
  swr_free (&transcoder->encoder->swr);
  avcodec_free_context (&transcoder->decoder->codec_context);
  avcodec_free_context (&transcoder->encoder->codec_context);
  g_free (transcoder->decoder);
  g_free (transcoder->encoder);
  g_free (transcoder);
}

gint
transcoder_decode (AudioTranscoder *transcoder, guint8 *input,
                   gint input_samples, gfloat *output)
{
  AudioTranscoderContext *decoder = transcoder->decoder;
  guint8 *output_ptr = (guint8 *)output;

  enum AVSampleFormat out_sample_format;
  av_opt_get_sample_fmt (decoder->swr, "out_sample_fmt", 0,
                         &out_sample_format);
  AVChannelLayout out_chlayout;
  av_opt_get_chlayout (decoder->swr, "out_chlayout", 0, &out_chlayout);

  gint bytes_per_raw_sample = decoder->codec_context->bits_per_raw_sample >> 3;
  if (bytes_per_raw_sample == 0)
    bytes_per_raw_sample = 1;
  bytes_per_raw_sample *= decoder->codec_context->ch_layout.nb_channels;
  gint input_size = input_samples * bytes_per_raw_sample;

  gint bytes_per_transcoded_sample
      = av_get_bytes_per_sample (out_sample_format) * out_chlayout.nb_channels;

  decoder->pkt = av_packet_alloc ();
  if (decoder->pkt == NULL)
    return AVERROR (ENOMEM);
  gint ret = av_new_packet (decoder->pkt, input_size);
  if (ret < 0)
    return ret;
  memcpy (decoder->pkt->data, input, input_size);

  ret = avcodec_send_packet (decoder->codec_context, decoder->pkt);
  if (ret < 0)
    return ret;

  decoder->frame = av_frame_alloc ();
  AVFrame *frame = decoder->frame;
  if (frame == NULL)
    {
      av_packet_free (&decoder->pkt);
      av_channel_layout_uninit (&out_chlayout);
      return AVERROR (ENOMEM);
    }

  while (ret >= 0)
    {
      ret = avcodec_receive_frame (decoder->codec_context, frame);
      if (ret < 0)
        break;

      ret = swr_convert (decoder->swr, &output_ptr, frame->nb_samples,
                         (const guint8 *const *)frame->data,
                         frame->nb_samples);
      if (ret < 0)
        break;

      output_ptr += frame->nb_samples * bytes_per_transcoded_sample;
    }

  // Flush decoder. Assumes that no buffering occured in the decoder and there
  // are no more frames
  avcodec_send_packet (decoder->codec_context, NULL);

  av_frame_free (&decoder->frame);
  av_packet_free (&decoder->pkt);
  av_channel_layout_uninit (&out_chlayout);

  if (ret < 0 && ret != AVERROR (EAGAIN) && ret != AVERROR_EOF)
    return ret;

  return 0;
}

gint
transcoder_encode (AudioTranscoder *transcoder, gfloat *input,
                   gint input_samples, guint8 *output)
{
  AudioTranscoderContext *encoder = transcoder->encoder;
  guint8 *output_ptr = output;

  enum AVSampleFormat sample_format;
  av_opt_get_sample_fmt (encoder->swr, "out_sample_fmt", 0, &sample_format);
  AVChannelLayout chlayout;
  av_opt_get_chlayout (encoder->swr, "out_chlayout", 0, &chlayout);

  AVFrame *frame = av_frame_alloc ();
  if (frame == NULL)
    return AVERROR (ENOMEM);
  encoder->frame = frame;
  frame->format = sample_format;
  frame->nb_samples = input_samples;
  frame->ch_layout = chlayout;
  av_frame_get_buffer (frame, 0);

  gint ret = swr_convert (encoder->swr, frame->data, frame->nb_samples,
                          (const guint8 *const *)&input, input_samples);
  if (ret < 0)
    return ret;

  ret = avcodec_send_frame (encoder->codec_context, frame);
  if (ret < 0)
    return ret;
  encoder->pkt = av_packet_alloc ();
  if (encoder->pkt == NULL)
    {
      av_frame_free (&encoder->frame);
      av_channel_layout_uninit (&chlayout);
      return AVERROR (ENOMEM);
    }

  while (ret >= 0)
    {
      ret = avcodec_receive_packet (encoder->codec_context, encoder->pkt);

      if (ret < 0)
        break;

      memcpy (output_ptr, encoder->pkt->data, encoder->pkt->size);
      output_ptr += encoder->pkt->size;
    }

  // Flush encoder. Assumes that no buffering occured in the encoder and there
  // are no more frames
  avcodec_send_frame (encoder->codec_context, NULL);

  av_frame_free (&encoder->frame);
  av_packet_free (&encoder->pkt);
  av_channel_layout_uninit (&chlayout);

  if (ret < 0 && ret != AVERROR (EAGAIN) && ret != AVERROR_EOF)
    return ret;

  return 0;
}
