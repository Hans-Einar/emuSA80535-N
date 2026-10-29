/* Compile-only target consumer; links no not-yet-implemented query symbol. */
#include <stddef.h>
#include <emusa80535/contracts/control3.h>
#define PREFIX_CHECK(T) \
 typedef char T##_size_offset[(offsetof(struct T,struct_size)==0)?1:-1]; \
 typedef char T##_version_offset[(offsetof(struct T,api_version)==4)?1:-1]; \
 typedef char T##_flags_offset[(offsetof(struct T,flags)==6)?1:-1]
PREFIX_CHECK(emuif_observed_value);
PREFIX_CHECK(emuif_observation);
PREFIX_CHECK(emuif_observer);
PREFIX_CHECK(emuif_observation_binding);
PREFIX_CHECK(emuif_observed_input);
PREFIX_CHECK(emuif_input_receipt);
PREFIX_CHECK(emuif_observation_api);
PREFIX_CHECK(emuif_trace_selector);
PREFIX_CHECK(emuif_trace_condition);
PREFIX_CHECK(emuif_trace_action);
PREFIX_CHECK(emuif_trace_watch);
PREFIX_CHECK(emuif_trace_point);
PREFIX_CHECK(emuif_trace_gate);
PREFIX_CHECK(emuif_trace_session);
PREFIX_CHECK(emuif_trace_destination);
PREFIX_CHECK(emuif_trace_array);
PREFIX_CHECK(emuif_trace_config);
PREFIX_CHECK(emuif_trace_revision);
PREFIX_CHECK(emuif_trace_record);
PREFIX_CHECK(emuif_trace_page_request);
PREFIX_CHECK(emuif_trace_page);
PREFIX_CHECK(emuif_capture_request);
PREFIX_CHECK(emuif_capture_status);
PREFIX_CHECK(emuif_sink_request);
PREFIX_CHECK(emuif_sink_status);
PREFIX_CHECK(emuif_trace_api);
PREFIX_CHECK(emuif_control_advance);
PREFIX_CHECK(emuif_control_progress);
PREFIX_CHECK(emuif_control3_api);
typedef char generation_is_64[(sizeof(((struct emuif_observation*)0)->generation)==8)?1:-1];
typedef char sequence_is_64[(sizeof(((struct emuif_trace_record*)0)->record_sequence)==8)?1:-1];
typedef char legacy_wire_preserved[(EMUIF_WIRE_MAJOR==2 && EMUIF_WIRE_MINOR==1)?1:-1];
typedef char target_wire_separate[(EMUIF_TARGET_WIRE_MAJOR==3 && EMUIF_TARGET_WIRE_MINOR==0)?1:-1];
int main(void) {
    struct emuif_observation_api table = {0};
    table.struct_size = sizeof(table);
    table.api_version = EMUIF_OBSERVATION_API_VERSION;
    return table.flags != 0 || EMUIF_OBSERVATION_IMPLEMENTED || EMUIF_CONTROL3_IMPLEMENTED;
}
