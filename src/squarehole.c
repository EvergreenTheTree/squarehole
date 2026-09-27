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

#define GETTEXT_PACKAGE "squarehole"
#include <glib/gi18n-lib.h>
#include <libavutil/frame.h>
#include <libavutil/mem.h>
#include <libavutil/opt.h>
#include <libavcodec/avcodec.h>
#include <libswresample/swresample.h>

#define TUTORIAL \
"# uncomment a set of lines below by removing the\n"\
"# leading to test and modify an example, use\n"\
"# use ctrl+a before typing to select all, if you\n"\
"# want a blank slate.\n"\
"#\n"\
"id=in # name a reference to the input buffer 'in'\n"\
"\n"\
"\n"\
"# adaptive threshold:\n"\
"#\n"\
"#threshold aux=[ ref=in gaussian-blur  std-dev-x=0.2rel std-dev-y=0.2rel ]\n"

// clang-format off
#ifdef GEGL_PROPERTIES

property_string (pipeline, _("pipeline"), TUTORIAL)
    description(_("[op [property=value] [property=value]] [[op] [property=value]"))
    ui_meta ("multiline", "true")

property_string (error, _("Eeeeeek"), "")
    description (_("There is a problem in the syntax or in the application of parsed property values. Things might mostly work nevertheless."))
    ui_meta ("error", "true")

property_enum (direction, "Processing direction",
               GeglOrientation, gegl_orientation,
               GEGL_ORIENTATION_HORIZONTAL)
    description (_("Whether to send image data through the audio pipline row by row or column by column"))

property_boolean (reverse_order, "Reverse line order", FALSE)
    description (_("The processing order of rows or columns is normally top to bottom or left to right respectively. This property reverses that when set to true."))

property_boolean (reverse_time, "Reverse time", FALSE)
    description (_("The processing order within rows and columns is normally left to right or top to bottom respectively. This property reverses that when set to true."))

property_boolean (bleed, "Bleed", TRUE)
    description (_("When set to false, sends each row or column through the audio pipeline in isolation. Defaults to true, which means processing from one line can affect the next."))

// TODO: Make the pixel format configurable beyond RGB u8
// TODO: Make the sample rate configurable
// TODO: Make the audio encoding configurable

// clang-format on
#else

#define GEGL_OP_FILTER
#define GEGL_OP_NAME     squarehole
#define GEGL_OP_C_SOURCE squarehole.c

#include "gegl-op.h"
#include "transcode.h"

typedef struct
{
  AudioTranscoder *transcoder;
} State;

static void
attach (GeglOperation *operation)
{
  // These allocations only need to happen once
  GeglProperties *o = GEGL_PROPERTIES (operation);
  State *state = g_new(State, 1);
  o->user_data = state;
  state->transcoder = alloc_transcoder();
}

static void
prepare (GeglOperation *operation)
{
  GeglProperties *o = GEGL_PROPERTIES (operation);
  State *state = o->user_data;
  // TODO: reconfigure ffmpeg encoder and resampler based on selected options (may require more allocations)
  // TODO: parse chain DSL, set any error messages and
  // TODO: allocate airwindows things (this will require some kinda wrapper library)
  // TODO: reset state of decoder / encoder, codec, codec parser

#if GEGL_MAJOR_VERSION >= 4
  const Babl *space = gegl_operation_get_source_space (operation, "input");

  gegl_operation_set_format (operation, "input", babl_format_with_space ("RGB u8", space));
  gegl_operation_set_format (operation, "output", babl_format_with_space ("RGB u8", space));
#else
  gegl_operation_set_format (operation, "input",  babl_format ("RGB u8"));
  gegl_operation_set_format (operation, "output", babl_format ("RGB u8"));
#endif
}

static GeglRectangle
get_cached_region (GeglOperation       *self,
                   const GeglRectangle *roi)
{
  const GeglRectangle *in_rect
      = gegl_operation_source_get_bounding_box (self, "input");

  if (in_rect && !gegl_rectangle_is_infinite_plane (in_rect))
    return *in_rect;

  return *roi;
}

static GeglRectangle
get_required_for_output (GeglOperation       *self,
                         const gchar         *input_pad,
                         const GeglRectangle *roi)
{
  return get_cached_region (self, roi);
}

static gboolean
process (GeglOperation       *operation,
         GeglBuffer          *input,
         GeglBuffer          *output,
         const GeglRectangle *result,
         gint                 level)
  {
  GeglProperties   *o = GEGL_PROPERTIES (operation);
  const Babl       *format = gegl_operation_get_format (operation, "output");
  gint              num_lines, length, line_num, j;
  GeglRectangle     line_rect;
  State            *state = (State *) o->user_data;
  AudioTranscoder  *transcoder = state->transcoder;

  if (o->direction == GEGL_ORIENTATION_HORIZONTAL)
    {
      num_lines = result->height;
      length = result->width;
      line_rect.width  = length;
      line_rect.height = 1;
    }
  else
    {
      num_lines = result->width;
      length = result->height;
      line_rect.width  = 1;
      line_rect.height = length;
    }

  gint bpp = babl_format_get_bytes_per_pixel (gegl_operation_get_format (operation, "input"));
  guint8 *line_buf = (guint8 *) g_new (guint8, length * bpp);
  enum AVSampleFormat sample_format;
  av_opt_get_sample_fmt (transcoder->decoder->swr, "in_sample_fmt", 0, &sample_format);
  gint bps = av_get_bytes_per_sample (sample_format);
  if (bpp % bps != 0) {
    // TODO, this would leave unprocessed pixels since bps < bpp
  }

  line_rect.x = result->x;
  line_rect.y = result->y;

  // TODO: respect direction option
  for (line_num = 0; line_num < num_lines; line_num++)
    {
      gegl_buffer_get (input, &line_rect, 1.0, format, (guint8 *) line_buf,
                       GEGL_AUTO_ROWSTRIDE, GEGL_ABYSS_NONE);
      // transcoder_decode(transcoder, line_buf, length, audio_bytes);

      // TODO: pass audio input buffer through effects pipeline (how to even process a graph like this idk should be fun)
      // TODO: use transcoder_encode
      // TODO: if bleed option is disabled, reset all audio effect state between lines

      // perform operation per image line and store it in place in line_buf
      gegl_buffer_set (output, &line_rect, 0, format, (guint8 *) line_buf,
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

  g_free(line_buf);

  return TRUE;
}

static gboolean
operation_process (GeglOperation        *operation,
                   GeglOperationContext *context,
                   const gchar          *output_prop,
                   const GeglRectangle  *result,
                   gint                  level)
{
  gboolean         success = FALSE;

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
    State *user_data = (State *) o->user_data;
    free_transcoder(user_data->transcoder);
    g_free(o->user_data);
  }
  G_OBJECT_CLASS (gegl_op_parent_class)->dispose (object);
}

static void
gegl_op_class_init (GeglOpClass *klass)
{
  GeglOperationClass       *operation_class;
  GeglOperationFilterClass *filter_class;

  operation_class = GEGL_OPERATION_CLASS (klass);
  filter_class    = GEGL_OPERATION_FILTER_CLASS (klass);

  operation_class->attach = attach;
  operation_class->prepare = prepare;
  operation_class->process = operation_process;
  operation_class->get_required_for_output = get_required_for_output;
  operation_class->get_cached_region = get_cached_region;
  G_OBJECT_CLASS(operation_class)->dispose = dispose;
  filter_class->process    = process;

  gegl_operation_class_set_keys (operation_class,
    "name",        "gegl:squarehole",
    "title",       "Square Hole",
    "categories",  "distort",
    "license",     "GPL3+",
    "description", "Fits a circle (image data) into a square hole (audio effects)",
    NULL);
}


#endif
