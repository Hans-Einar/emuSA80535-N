/* Opaque controller lifecycle seam, same build only. Copyright 2026 Hans-Einar. MIT. */
#ifndef EMU_CPU_LIFECYCLE_INTERNAL_H
#define EMU_CPU_LIFECYCLE_INTERNAL_H
#include <emusa80535/contracts/cpu.h>
emuif_status emu_cpu_observation_lifecycle(struct emuif_cpu *, uint32_t kind,
                                           emuif_status status);
#endif
