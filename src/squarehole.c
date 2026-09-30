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

#define GETTEXT_PACKAGE "squarehole"
#include <glib/gi18n-lib.h>
#include <libavcodec/avcodec.h>
#include <libavutil/frame.h>
#include <libavutil/mem.h>
#include <libavutil/opt.h>
#include <libswresample/swresample.h>

// Uses the age-old X macro trick, redefine X to make use of this list of
// encodings. The all caps identifier is intended to refer to the suffixes
// following the AV_CODEC_ID_ macros defined by libavcodec
#define SUPPORTED_AUDIO_ENCODINGS                                             \
  X (PCM_ALAW, "pcm_alaw", N_ ("PCM A-law / G.711 A-law"))                    \
  X (PCM_F32BE, "pcm_f32be", N_ ("PCM 32-bit floating point big-endian"))     \
  X (PCM_F32LE, "pcm_f32le", N_ ("PCM 32-bit floating point little-endian"))  \
  X (PCM_F64BE, "pcm_f64be", N_ ("PCM 64-bit floating point big-endian"))     \
  X (PCM_F64LE, "pcm_f64le", N_ ("PCM 64-bit floating point little-endian"))  \
  X (PCM_MULAW, "pcm_mulaw", N_ ("PCM mu-law / G.711 mu-law"))                \
  X (PCM_S8, "pcm_s8", N_ ("PCM signed 8-bit"))                               \
  X (PCM_S16BE, "pcm_s16be", N_ ("PCM signed 16-bit big-endian"))             \
  X (PCM_S16LE, "pcm_s16le", N_ ("PCM signed 16-bit little-endian"))          \
  X (PCM_S24BE, "pcm_s24be", N_ ("PCM signed 24-bit big-endian"))             \
  X (PCM_S24LE, "pcm_s24le", N_ ("PCM signed 24-bit little-endian"))          \
  X (PCM_S32BE, "pcm_s32be", N_ ("PCM signed 32-bit big-endian"))             \
  X (PCM_S32LE, "pcm_s32le", N_ ("PCM signed 32-bit little-endian"))          \
  X (PCM_S64BE, "pcm_s64be", N_ ("PCM signed 64-bit big-endian"))             \
  X (PCM_S64LE, "pcm_s64le", N_ ("PCM signed 64-bit little-endian"))          \
  X (PCM_U8, "pcm_u8", N_ ("PCM unsigned 8-bit"))                             \
  X (PCM_U16BE, "pcm_u16be", N_ ("PCM unsigned 16-bit big-endian"))           \
  X (PCM_U16LE, "pcm_u16le", N_ ("PCM unsigned 16-bit little-endian"))        \
  X (PCM_U24BE, "pcm_u24be", N_ ("PCM unsigned 24-bit big-endian"))           \
  X (PCM_U24LE, "pcm_u24le", N_ ("PCM unsigned 24-bit little-endian"))        \
  X (PCM_U32BE, "pcm_u32be", N_ ("PCM unsigned 32-bit big-endian"))           \
  X (PCM_U32LE, "pcm_u32le", N_ ("PCM unsigned 32-bit little-endian"))

#define TUTORIAL                                                              \
  "# uncomment a set of lines below by removing the\n"                        \
  "# leading to test and modify an example, use\n"                            \
  "# use ctrl+a before typing to select all, if you\n"                        \
  "# want a blank slate.\n"                                                   \
  "#\n"                                                                       \
  "id=in # name a reference to the input buffer 'in'\n"                       \
  "\n"                                                                        \
  "\n"                                                                        \
  "# adaptive threshold:\n"                                                   \
  "#\n"                                                                       \
  "#threshold aux=[ ref=in gaussian-blur  std-dev-x=0.2rel std-dev-y=0.2rel " \
  "]\n"

// clang-format off
#ifdef GEGL_PROPERTIES

property_string (pipeline, _("Audio Pipeline"), TUTORIAL)
    description(_("[op [property=value] [property=value]] [[op] [property=value]"))
    ui_meta ("multiline", "true")

property_string (error, _("Eeeeeek"), "")
    description (_("There is a problem in the syntax or in the application of parsed property values. Things might mostly work nevertheless."))
    ui_meta ("error", "true")

property_enum (direction, _("Processing direction"),
               GeglOrientation, gegl_orientation,
               GEGL_ORIENTATION_HORIZONTAL)
    description (_("Whether to send image data through the audio pipline row by row or column by column"))

property_boolean (reverse_order, _("Reverse line order"), FALSE)
    description (_("The processing order of rows or columns is normally top to bottom or left to right respectively. This property reverses that when set to true."))

property_boolean (reverse_time, _("Reverse time"), FALSE)
    description (_("The processing order within rows and columns is normally left to right or top to bottom respectively. This property reverses that when set to true."))

property_boolean (bleed, _("Bleed"), TRUE)
    description (_("When set to false, sends each row or column through the audio pipeline in isolation. Defaults to true, which means processing from one line can affect the next."))

property_int (sample_rate, _("Sample rate"), 44100)
    description (_("Sample rate to run the audio effects at. Generally affects audio effects that have a time-based component (delay, reverb, etc.)"))

// cursed, see SUPPORTED_AUDIO_ENCODINGS definition
enum_start (gegl_squarehole_audio_encoding)
#define X(val, name, desc) enum_value (GEGL_SQUAREHOLE_AUDIO_ENC_ ## val , name, desc)
SUPPORTED_AUDIO_ENCODINGS
#undef X
enum_end (GeglSquareholeAudioEncoding)

property_enum (audio_encoding, _("Audio encoding"),
              GeglSquareholeAudioEncoding, gegl_squarehole_audio_encoding, GEGL_SQUAREHOLE_AUDIO_ENC_PCM_MULAW)
              description (_("What sample format / audio encoding to treat the pixel data as. Defaults to mu-law since it is an 8-bit encoding and produces nice results."))

enum_start (gegl_squarehole_pixel_format)
  enum_value (GEGL_SQUAREHOLE_PX_FMT_RGB_U8, "RGB u8", N_ ("RGB linear as 8-bit unsigned integers"))
  enum_value (GEGL_SQUAREHOLE_PX_FMT_RGBA_U8, "RGBA u8", N_ ("RGB linear, separate alpha as 8-bit unsigned integers"))
  enum_value (GEGL_SQUAREHOLE_PX_FMT_RAGABAA_U8, "RaGaBaA u8", N_ ("RGB linear, associated alpha as 8-bit unsigned integers"))
  enum_value (GEGL_SQUAREHOLE_PX_FMT_CMYK_U8, "CMYK u8", N_ ("CMYK as 8-bit unsigned integers"))
  enum_value (GEGL_SQUAREHOLE_PX_FMT_CMYKA_U8, "CMYKA u8", N_ ("CMYK, separate alpha as 8-bit unsigned integers"))
enum_end (GeglSquareholePixelFormat)

property_enum (pixel_format, _("Pixel format"),
              GeglSquareholePixelFormat, gegl_squarehole_pixel_format, GEGL_SQUAREHOLE_PX_FMT_RGB_U8)
              description (_("What pixel format to convert image data to before running through the decoder and audio effects. Defaults to RGB u8."))

// clang-format on
#else

#define GEGL_OP_FILTER
#define GEGL_OP_NAME squarehole
#define GEGL_OP_C_SOURCE squarehole.c

#include "gegl-op.h"
#include "transcode.h"

typedef struct
{
  Transcoder *transcoder;
} State;

// cursed, see SUPPORTED_AUDIO_ENCODINGS definition
enum AVCodecID
encoding_property_to_codec (GeglSquareholeAudioEncoding encoding)
{
  switch (encoding)
    {
      // clang-format off
#define X(val, name, desc) case GEGL_SQUAREHOLE_AUDIO_ENC_ ## val: return AV_CODEC_ID_ ## val;
SUPPORTED_AUDIO_ENCODINGS
#undef X
      // clang-format on
    }
}

const gchar *
pixel_format_property_to_babl_format (GeglSquareholePixelFormat pixel_format)
{
  GEnumClass *enum_class
      = g_type_class_ref (gegl_squarehole_pixel_format_get_type ());
  GEnumValue *enum_value = g_enum_get_value (enum_class, pixel_format);

  g_type_class_unref (enum_class);
  return enum_value->value_name;
}

static void
attach (GeglOperation *operation)
{
  // These allocations only need to happen once
  GeglProperties *o = GEGL_PROPERTIES (operation);
  State *state = g_new (State, 1);
  o->user_data = state;
  state->transcoder = transcoder_alloc ();
}

static void
prepare (GeglOperation *operation)
{
  GeglProperties *o = GEGL_PROPERTIES (operation);
  State *state = o->user_data;
  transcoder_configure (state->transcoder,
                        encoding_property_to_codec (o->audio_encoding));
  const gchar *pixel_format
      = pixel_format_property_to_babl_format (o->pixel_format);
  // TODO: parse chain DSL, set any error messages and
  // TODO: allocate airwindows things (this will require some kinda wrapper
  // library)

#if GEGL_MAJOR_VERSION >= 4
  const Babl *space = gegl_operation_get_source_space (operation, "input");

  gegl_operation_set_format (operation, "input",
                             babl_format_with_space (pixel_format, space));
  gegl_operation_set_format (operation, "output",
                             babl_format_with_space (pixel_format, space));
#else
  gegl_operation_set_format (operation, "input", babl_format (pixel_format));
  gegl_operation_set_format (operation, "output", babl_format (pixel_format));
#endif
}

static GeglRectangle
get_cached_region (GeglOperation *self, const GeglRectangle *roi)
{
  const GeglRectangle *in_rect
      = gegl_operation_source_get_bounding_box (self, "input");

  if (in_rect && !gegl_rectangle_is_infinite_plane (in_rect))
    return *in_rect;

  return *roi;
}

static GeglRectangle
get_required_for_output (GeglOperation *self, const gchar *input_pad,
                         const GeglRectangle *roi)
{
  return get_cached_region (self, roi);
}

static gboolean
process (GeglOperation *operation, GeglBuffer *input, GeglBuffer *output,
         const GeglRectangle *result, gint level)
{
  GeglProperties *o = GEGL_PROPERTIES (operation);
  const Babl *format = gegl_operation_get_format (operation, "output");
  gint num_lines, length, line_num;
  GeglRectangle line_rect;
  State *state = (State *)o->user_data;
  Transcoder *transcoder = state->transcoder;

  if (o->direction == GEGL_ORIENTATION_HORIZONTAL)
    {
      num_lines = result->height;
      length = result->width;
      line_rect.width = length;
      line_rect.height = 1;
    }
  else
    {
      num_lines = result->width;
      length = result->height;
      line_rect.width = 1;
      line_rect.height = length;
    }

  gint bpp = babl_format_get_bytes_per_pixel (
      gegl_operation_get_format (operation, "input"));
  guint8 *line_buf = (guint8 *)g_new (guint8, length * bpp);
  enum AVSampleFormat sample_format;
  av_opt_get_sample_fmt (transcoder->decoder->swr, "in_sample_fmt", 0,
                         &sample_format);
  gint bps = av_get_bytes_per_sample (sample_format);
  if (bpp % bps != 0)
    {
      // TODO, this would leave unprocessed pixels since bps < bpp
    }
  gint n_samples = (length * bpp) / bps;

  line_rect.x = result->x;
  line_rect.y = result->y;

  // TODO: respect direction option
  for (line_num = 0; line_num < num_lines; line_num++)
    {
      gegl_buffer_get (input, &line_rect, 1.0, format, (guint8 *)line_buf,
                       GEGL_AUTO_ROWSTRIDE, GEGL_ABYSS_NONE);
      gfloat *audio_samples = (gfloat *)g_new (gfloat, n_samples);
      transcoder_decode (transcoder, line_buf, n_samples, audio_samples);

      // TODO: pass audio input buffer through effects pipeline (how to even
      // process a graph like this idk should be fun)
      // TODO: if bleed option is disabled, reset all audio effect state
      // between lines

      transcoder_encode (transcoder, audio_samples, n_samples, line_buf);

      // perform operation per image line and store it in place in line_buf
      gegl_buffer_set (output, &line_rect, 0, format, (guint8 *)line_buf,
                       GEGL_AUTO_ROWSTRIDE);

      // TODO: respect direction option
      if (o->direction == GEGL_ORIENTATION_HORIZONTAL)
        {
          line_rect.y++;
        }
      else
        {
          line_rect.x++;
        }
    }

  g_free (line_buf);
  line_buf = NULL;

  return TRUE;
}

static gboolean
operation_process (GeglOperation *operation, GeglOperationContext *context,
                   const gchar *output_prop, const GeglRectangle *result,
                   gint level)
{
  gboolean success = FALSE;

  const GeglRectangle *in_rect
      = gegl_operation_source_get_bounding_box (operation, "input");

  if (in_rect && gegl_rectangle_is_infinite_plane (in_rect))
    {
      gpointer in = gegl_operation_context_get_object (context, "input");
      gegl_operation_context_take_object (context, "output",
                                          g_object_ref (G_OBJECT (in)));
      return TRUE;
    }
  else
    {
      GeglOperationFilterClass *klass;
      GeglBuffer *input;
      GeglBuffer *output;

      if (strcmp (output_prop, "output"))
        {
          g_warning ("requested processing of %s pad on a filter",
                     output_prop);
          return FALSE;
        }

      input
          = (GeglBuffer *)gegl_operation_context_dup_object (context, "input");
      output = gegl_operation_context_get_output_maybe_in_place (
          operation, context, input, result);
      klass = GEGL_OPERATION_FILTER_GET_CLASS (operation);
      success = klass->process (operation, input, output, result, level);

      g_clear_object (&input);
    }

  return success;
}

static void
dispose (GObject *object)
{
  GeglProperties *o = GEGL_PROPERTIES (object);

  if (o != NULL)
    {
      State *user_data = (State *)o->user_data;
      transcoder_free (&user_data->transcoder);
      g_free (user_data);
      o->user_data = NULL;
    }
  G_OBJECT_CLASS (gegl_op_parent_class)->dispose (object);
}

static void
gegl_op_class_init (GeglOpClass *klass)
{
  GeglOperationClass *operation_class;
  GeglOperationFilterClass *filter_class;

  operation_class = GEGL_OPERATION_CLASS (klass);
  filter_class = GEGL_OPERATION_FILTER_CLASS (klass);

  operation_class->attach = attach;
  operation_class->prepare = prepare;
  operation_class->process = operation_process;
  operation_class->get_required_for_output = get_required_for_output;
  operation_class->get_cached_region = get_cached_region;
  G_OBJECT_CLASS (operation_class)->dispose = dispose;
  filter_class->process = process;

  // clang-format off
  gegl_operation_class_set_keys (operation_class,
    "name",        "gegl:squarehole",
    "title",       "Square Hole",
    "categories",  "distort",
    "license",     "GPL3+",
    "description", "Fits a circle (image data) into a square hole (audio effects)",
    NULL);
  // clang-format on
}

#endif
