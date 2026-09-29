#include <AirwinRegistry.h>

AirwinConsolidatedBase *
airwin_get_effect (const char *name)
{
  auto effect_index_it = AirwinRegistry::nameToIndex.find (name);
  if (effect_index_it == AirwinRegistry::nameToIndex.end ())
    {
      return NULL;
    }
  AirwinRegistry::awReg reg_entry
      = AirwinRegistry::registry[effect_index_it->second];
  return reg_entry.generator ().release ();
}

// TODO still don't know what form airwin2rack effects take f32 samples in 
//   (planar or not)
// TODO probably don't need this to be C wrapper, we can save that for the
//   general audio module which would expose the function to turn the input
//   to a processing graph which f32 samples can be run through
// TODO generic audio effect wrapper for custom fx, airwindows effects,
//   ffmpeg audio effects
// TODO None of the airwindows effects take aux inputs which means processing
//   chains can only be linear (minus the dry / wet controls), make various
//   combining effects (blend, add, mix, divide, multiply, bitwise ops, etc.)
// TODO audio processing graph data structure (and all the equality and
//   printing methods)
// TODO chain syntax lexer / parser that creates the above graph
// TODO airwin2rack does not support there being more than one effect instance,
//   maybe make an issue about it
