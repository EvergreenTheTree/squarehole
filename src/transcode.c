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

Transcoder *
transcoder_alloc (void)
{
  Transcoder *transcoder = g_new (Transcoder, 1);
  if (transcoder == NULL)
    return NULL;

  TranscoderContext *encoder = g_new (TranscoderContext, 1);
  if (encoder == NULL)
    {
      g_free (transcoder);
      return NULL;
    }
  transcoder->encoder = encoder;
  TranscoderContext *decoder = g_new (TranscoderContext, 1);
  if (decoder == NULL)
    {
      g_free (transcoder);
      g_free (encoder);
      return NULL;
    }
  transcoder->decoder = decoder;
  transcoder->configured = FALSE;
  return transcoder;
}

gint
transcoder_configure (Transcoder *transcoder, enum AVCodecID raw_codec)
{
  gint ret;

  TranscoderContext *decoder = transcoder->decoder;
  TranscoderContext *encoder = transcoder->encoder;

  const AVCodec *decoder_codec = avcodec_find_decoder (raw_codec);
  decoder->codec = decoder_codec;
  if (transcoder->configured)
    avcodec_free_context (&transcoder->decoder->codec_context);
  decoder->codec_context = avcodec_alloc_context3 (decoder_codec);
  if (decoder->codec_context == NULL)
    return AVERROR (ENOMEM);
  decoder->codec_context->sample_rate = 44100;
  decoder->codec_context->ch_layout = (AVChannelLayout)AV_CHANNEL_LAYOUT_MONO;
  ret = avcodec_open2 (decoder->codec_context, decoder_codec, NULL);
  if (ret < 0)
    {
      avcodec_free_context (&decoder->codec_context);
      return ret;
    }

  const AVCodec *encoder_codec = avcodec_find_encoder (raw_codec);
  encoder->codec = encoder_codec;
  if (transcoder->configured)
    avcodec_free_context (&transcoder->encoder->codec_context);
  encoder->codec_context = avcodec_alloc_context3 (encoder_codec);

  if (encoder->codec_context == NULL)
    {
      avcodec_free_context (&decoder->codec_context);
      return AVERROR (ENOMEM);
    }
  encoder->codec_context->sample_rate = 44100;
  encoder->codec_context->ch_layout = (AVChannelLayout)AV_CHANNEL_LAYOUT_MONO;

  // Determine the first supported sample format
  const enum AVSampleFormat *sample_fmts;
  int num_sample_fmts;
  ret = avcodec_get_supported_config (
      NULL, encoder_codec, AV_CODEC_CONFIG_SAMPLE_FORMAT, 0,
      (const void **)&sample_fmts, &num_sample_fmts);
  if (ret < 0 || num_sample_fmts == 0)
    {
      avcodec_free_context (&decoder->codec_context);
      avcodec_free_context (&encoder->codec_context);
      return ret;
    }

  encoder->codec_context->sample_fmt = sample_fmts[0];
  ret = avcodec_open2 (encoder->codec_context, encoder_codec, NULL);
  if (ret < 0)
    {
      avcodec_free_context (&decoder->codec_context);
      avcodec_free_context (&encoder->codec_context);
      return ret;
    }

  if (!transcoder->configured)
    {
      decoder->swr = swr_alloc ();
      if (decoder->swr == NULL)
        {
          avcodec_free_context (&decoder->codec_context);
          avcodec_free_context (&encoder->codec_context);
          return AVERROR (ENOMEM);
        }
    }
  av_opt_set_chlayout (decoder->swr, "in_chlayout",
                       &(AVChannelLayout)AV_CHANNEL_LAYOUT_MONO, 0);
  av_opt_set_chlayout (decoder->swr, "out_chlayout",
                       &(AVChannelLayout)AV_CHANNEL_LAYOUT_MONO, 0);
  av_opt_set_int (decoder->swr, "in_sample_rate", 44100, 0);
  av_opt_set_int (decoder->swr, "out_sample_rate", 44100, 0);
  av_opt_set_sample_fmt (decoder->swr, "in_sample_fmt",
                         encoder->codec_context->sample_fmt, 0);
  av_opt_set_sample_fmt (decoder->swr, "out_sample_fmt", AV_SAMPLE_FMT_FLT, 0);
  ret = swr_init (decoder->swr);
  if (ret < 0)
    {
      avcodec_free_context (&decoder->codec_context);
      avcodec_free_context (&encoder->codec_context);
      swr_free (&decoder->swr);
      return ret;
    }

  if (!transcoder->configured)
    {
      encoder->swr = swr_alloc ();
      if (encoder->swr == NULL)
        {
          avcodec_free_context (&decoder->codec_context);
          avcodec_free_context (&encoder->codec_context);
          swr_free (&decoder->swr);
          return AVERROR (ENOMEM);
        }
    }
  av_opt_set_chlayout (encoder->swr, "in_chlayout",
                       &(AVChannelLayout)AV_CHANNEL_LAYOUT_MONO, 0);
  av_opt_set_chlayout (encoder->swr, "out_chlayout",
                       &(AVChannelLayout)AV_CHANNEL_LAYOUT_MONO, 0);
  av_opt_set_int (encoder->swr, "in_sample_rate", 44100, 0);
  av_opt_set_int (encoder->swr, "out_sample_rate", 44100, 0);
  av_opt_set_sample_fmt (encoder->swr, "in_sample_fmt", AV_SAMPLE_FMT_FLT, 0);
  av_opt_set_sample_fmt (encoder->swr, "out_sample_fmt",
                         encoder->codec_context->sample_fmt, 0);
  ret = swr_init (encoder->swr);
  if (ret < 0)
    {
      avcodec_free_context (&decoder->codec_context);
      avcodec_free_context (&encoder->codec_context);
      swr_free (&decoder->swr);
      swr_free (&encoder->swr);
      return ret;
    }

  transcoder->configured = TRUE;
  return 0;
}

void
transcoder_free (Transcoder **transcoder)
{
  swr_free (&(*transcoder)->decoder->swr);
  swr_free (&(*transcoder)->encoder->swr);
  avcodec_free_context (&(*transcoder)->decoder->codec_context);
  avcodec_free_context (&(*transcoder)->encoder->codec_context);
  g_free ((*transcoder)->decoder);
  g_free ((*transcoder)->encoder);
  g_free (*transcoder);
  *transcoder = NULL;
}

gint
transcoder_decode (Transcoder *transcoder, guint8 *input, gint input_samples,
                   gfloat *output)
{
  TranscoderContext *decoder = transcoder->decoder;
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
    {
      av_packet_free (&decoder->pkt);
      av_channel_layout_uninit (&out_chlayout);
      return ret;
    }
  memcpy (decoder->pkt->data, input, input_size);

  ret = avcodec_send_packet (decoder->codec_context, decoder->pkt);
  if (ret < 0)
    {
      av_packet_free (&decoder->pkt);
      av_channel_layout_uninit (&out_chlayout);
      return ret;
    }

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

  av_frame_free (&decoder->frame);
  av_packet_free (&decoder->pkt);
  av_channel_layout_uninit (&out_chlayout);

  if (ret < 0 && ret != AVERROR (EAGAIN) && ret != AVERROR_EOF)
    return ret;

  return 0;
}

gint
transcoder_encode (Transcoder *transcoder, gfloat *input, gint input_samples,
                   guint8 *output)
{
  TranscoderContext *encoder = transcoder->encoder;
  guint8 *output_ptr = output;

  enum AVSampleFormat sample_format;
  av_opt_get_sample_fmt (encoder->swr, "out_sample_fmt", 0, &sample_format);
  AVChannelLayout chlayout;
  av_opt_get_chlayout (encoder->swr, "out_chlayout", 0, &chlayout);

  AVFrame *frame = av_frame_alloc ();
  if (frame == NULL)
    return AVERROR (ENOMEM);
      av_packet_free (&decoder->pkt);
      av_channel_layout_uninit (&out_chlayout);
  encoder->frame = frame;
  frame->format = sample_format;
  frame->nb_samples = input_samples;
  frame->ch_layout = chlayout;
  av_frame_get_buffer (frame, 0);

  gint ret = swr_convert (encoder->swr, frame->data, frame->nb_samples,
                          (const guint8 *const *)&input, input_samples);
  if (ret < 0)
    {
      av_frame_free (&encoder->frame);
      av_channel_layout_uninit (&chlayout);
      return ret;
    }

  ret = avcodec_send_frame (encoder->codec_context, frame);
  if (ret < 0)
    {
      av_frame_free (&encoder->frame);
      av_channel_layout_uninit (&chlayout);
      return ret;
    }
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

  av_frame_free (&encoder->frame);
  av_packet_free (&encoder->pkt);
  av_channel_layout_uninit (&chlayout);

  if (ret < 0 && ret != AVERROR (EAGAIN) && ret != AVERROR_EOF)
    return ret;

  return 0;
}
