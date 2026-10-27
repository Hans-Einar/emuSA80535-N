/* Private host integration; not part of immutable SDK. Copyright 2026 Hans-Einar. MIT. */
#ifndef EMU_SESSION_INTERNAL_H
#define EMU_SESSION_INTERNAL_H
#include <emusa80535/contracts/runtime.h>
/* Host must obtain an explicit promise from the selected module's documentation
 * or metadata. Default false; valid only before clients open, paused at cycle0. */
emuif_status emu_session_allow_fault_inspection(struct emuif_session *, uint32_t);
#endif
