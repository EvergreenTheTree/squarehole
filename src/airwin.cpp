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

// TODO make C wrappers for all of the necessary AirwinConsolidatedBase methods
// TODO None of the airwindows effects take aux inputs which means processing
//   chains can only be linear (minus the dry / wet controls), make various
//   combining effects (blend, add, mix, divide, multiply, bitwise ops, etc.)
