#ifndef SQUAREHOLE_AIRWIN_H
#define SQUAREHOLE_AIRWIN_H

#ifdef __cplusplus
#include <AirwinRegistry.h>
extern "C" {
#else
typedef struct AirwinConsolidatedBase
#endif

AirwinConsolidatedBase *
airwin_get_effect(const char *name);

#ifdef __cplusplus
}
#endif

#endif /* SQUAREHOLE_AIRWIN_H */
