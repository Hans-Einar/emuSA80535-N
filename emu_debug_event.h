/* Generic deterministic debugger event and watch matching runtime.
 * Copyright 2026 Hans-Einar
 *
 * Distributed under the MIT license; see LICENSE.
 */

#ifndef EMU_DEBUG_EVENT_H
#define EMU_DEBUG_EVENT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define EM8051_DEBUG_EVENT_MAX_OBSERVERS 8u
#define EM8051_DEBUG_WATCH_MAX_WATCHES 64u
#define EM8051_DEBUG_WATCH_MAX_ACTIONS 8u
#define EM8051_DEBUG_WATCH_MAX_ROUTES 16u

enum em8051_debug_event_status
{
    EM8051_DEBUG_EVENT_OK = 0,
    EM8051_DEBUG_EVENT_INVALID_ARGUMENT,
    EM8051_DEBUG_EVENT_LIMIT,
    EM8051_DEBUG_EVENT_BUSY,
    EM8051_DEBUG_EVENT_DUPLICATE_ID
};

enum em8051_debug_event_kind
{
    EM8051_DEBUG_EVENT_INSTRUCTION_BEGIN = 0,
    EM8051_DEBUG_EVENT_INSTRUCTION_END,
    EM8051_DEBUG_EVENT_CODE_FETCH,
    EM8051_DEBUG_EVENT_MEMORY_READ,
    EM8051_DEBUG_EVENT_MEMORY_WRITE,
    EM8051_DEBUG_EVENT_MEMORY_RMW,
    EM8051_DEBUG_EVENT_CONTROL_CALL,
    EM8051_DEBUG_EVENT_CONTROL_RETURN,
    EM8051_DEBUG_EVENT_INTERRUPT_REQUEST,
    EM8051_DEBUG_EVENT_INTERRUPT_ENTER,
    EM8051_DEBUG_EVENT_INTERRUPT_EXIT,
    EM8051_DEBUG_EVENT_EXCEPTION,
    EM8051_DEBUG_EVENT_HALT,
    EM8051_DEBUG_EVENT_TIMER_OVERFLOW,
    EM8051_DEBUG_EVENT_UART,
    EM8051_DEBUG_EVENT_RESET,
    EM8051_DEBUG_EVENT_LOAD,
    EM8051_DEBUG_EVENT_DEBUGGER_MUTATION,
    EM8051_DEBUG_EVENT_WATCH_MATCH
};

enum em8051_debug_address_space
{
    EM8051_DEBUG_SPACE_NONE = 0,
    EM8051_DEBUG_SPACE_CODE,
    EM8051_DEBUG_SPACE_IRAM_LOWER,
    EM8051_DEBUG_SPACE_IRAM_UPPER,
    EM8051_DEBUG_SPACE_SFR,
    EM8051_DEBUG_SPACE_XDATA
};

enum em8051_debug_access_kind
{
    EM8051_DEBUG_ACCESS_NONE = 0,
    EM8051_DEBUG_ACCESS_FETCH = 1u << 0,
    EM8051_DEBUG_ACCESS_READ = 1u << 1,
    EM8051_DEBUG_ACCESS_WRITE = 1u << 2,
    EM8051_DEBUG_ACCESS_RMW = 1u << 3
};

struct em8051_debug_value
{
    uint64_t value;
    uint8_t width;
    uint8_t known;
    uint8_t is_signed;
    uint8_t reserved;
};

struct em8051_debug_event
{
    uint64_t sequence;
    uint64_t instruction_count;
    uint64_t machine_cycle_count;
    uint32_t generation;
    uint16_t pc;
    uint16_t address;
    uint8_t phase;
    uint8_t kind;
    uint8_t address_space;
    uint8_t access;
    struct em8051_debug_value old_value;
    struct em8051_debug_value new_value;
};

typedef void (*em8051_debug_event_observer)(
    const struct em8051_debug_event *aEvent, void *aUser);

struct em8051_debug_event_observer_slot
{
    em8051_debug_event_observer callback;
    void *user;
    uint32_t id;
};

struct em8051_debug_event_bus
{
    struct em8051_debug_event_observer_slot
        observers[EM8051_DEBUG_EVENT_MAX_OBSERVERS];
    uint64_t next_sequence;
    uint32_t next_observer_id;
    uint8_t observer_count;
    uint8_t dispatching;
};

void em8051_debug_event_bus_init(struct em8051_debug_event_bus *aBus);
enum em8051_debug_event_status em8051_debug_event_bus_subscribe(
    struct em8051_debug_event_bus *aBus,
    em8051_debug_event_observer aCallback, void *aUser,
    uint32_t *aObserverId);
enum em8051_debug_event_status em8051_debug_event_bus_unsubscribe(
    struct em8051_debug_event_bus *aBus, uint32_t aObserverId);
enum em8051_debug_event_status em8051_debug_event_bus_emit(
    struct em8051_debug_event_bus *aBus,
    const struct em8051_debug_event *aEvent,
    uint64_t *aAssignedSequence);

enum em8051_debug_watch_compare
{
    EM8051_DEBUG_WATCH_EQ = 0,
    EM8051_DEBUG_WATCH_NE,
    EM8051_DEBUG_WATCH_LT,
    EM8051_DEBUG_WATCH_LE,
    EM8051_DEBUG_WATCH_GT,
    EM8051_DEBUG_WATCH_GE
};

enum em8051_debug_watch_operand
{
    EM8051_DEBUG_WATCH_OLD = 0,
    EM8051_DEBUG_WATCH_NEW
};

enum em8051_debug_watch_action_kind
{
    EM8051_DEBUG_WATCH_STOP = 0,
    EM8051_DEBUG_WATCH_CONSOLE,
    EM8051_DEBUG_WATCH_QUIET,
    EM8051_DEBUG_WATCH_ROUTE
};

struct em8051_debug_watch_condition
{
    uint64_t constant;
    uint8_t present;
    uint8_t operand;
    uint8_t comparison;
    uint8_t width;
    uint8_t is_signed;
};

struct em8051_debug_watch_action
{
    struct em8051_debug_watch_condition condition;
    uint32_t trace_ids[EM8051_DEBUG_WATCH_MAX_ROUTES];
    uint8_t kind;
    uint8_t trace_id_count;
};

struct em8051_debug_watch
{
    struct em8051_debug_watch_action actions[EM8051_DEBUG_WATCH_MAX_ACTIONS];
    uint32_t id;
    uint16_t address_first;
    uint16_t address_last;
    uint8_t enabled;
    uint8_t address_space;
    uint8_t access_mask;
    uint8_t action_count;
};

struct em8051_debug_watch_result
{
    uint32_t trace_ids[EM8051_DEBUG_WATCH_MAX_ROUTES];
    uint32_t watch_id;
    uint32_t fired_action_mask;
    uint64_t source_event_sequence;
    uint8_t stop;
    uint8_t console;
    uint8_t quiet;
    uint8_t trace_id_count;
};

struct em8051_debug_watch_table
{
    struct em8051_debug_watch watches[EM8051_DEBUG_WATCH_MAX_WATCHES];
    uint8_t watch_count;
};

void em8051_debug_watch_table_init(struct em8051_debug_watch_table *aTable);
enum em8051_debug_event_status em8051_debug_watch_table_replace(
    struct em8051_debug_watch_table *aTable,
    const struct em8051_debug_watch *aWatches, size_t aCount);
enum em8051_debug_event_status em8051_debug_watch_table_match(
    const struct em8051_debug_watch_table *aTable,
    const struct em8051_debug_event *aEvent,
    struct em8051_debug_watch_result *aResults, size_t aCapacity,
    size_t *aRequired);

#endif
