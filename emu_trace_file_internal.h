/* Private same-build host sink adapter. Copyright 2026 Hans-Einar. MIT. */
#ifndef EMU_TRACE_FILE_INTERNAL_H
#define EMU_TRACE_FILE_INTERNAL_H
#include <emusa80535/contracts/trace.h>
struct emu_trace_file_metadata {
  const char *runtime_commit, *contract_manifest_sha256, *session_id;
  const char *image_sha256, *module_sha256, *config_sha256,
      *trace_config_sha256;
  const char *configuration_json; /* canonical sorted-key UTF-8; borrowed until
                                     open returns */
  uint32_t configuration_bytes;
  uint64_t generation;
};
struct emu_trace_file_provider {
  void *context;
  emuif_status (*open)(void *, const struct emuif_sink_request *,
                       const struct emu_trace_file_metadata *,
                       struct emuif_sink_status *);
  /* Already routed/coalesced immutable view. Backend owns file storage IDs;
   * delivery loss/failure is reported by status, never an architectural fault.
   * EXHAUSTED tells the owner to stop affected captures before ID reuse.
   */
  emuif_status (*enqueue)(void *, const struct emuif_trace_record *);
  emuif_status (*operation)(void *, uint32_t destination_id, uint32_t close,
                            uint64_t generation, struct emuif_sink_status *);
  emuif_status (*status)(void *, uint32_t destination_id,
                         struct emuif_sink_status *);
  uint32_t (*active)(void *);
  emuif_status (*close_all)(void *, uint64_t generation);
};
/* Session owner registration only before any clients/progress; callback table
 * is copied, context remains host-owned through session destruction. */
emuif_status
emu_session_set_file_provider(struct emuif_session *,
                              const struct emu_trace_file_provider *);
/* Same serialized owner, used by host shutdown as well as protocol termination.
 */
emuif_status emu_session_close_file_sinks(struct emuif_session *);
#endif
