#include "../emu_debug_event.h"

#include <stdio.h>
#include <string.h>

#define CHECK(expression) do { \
    if (!(expression)) { \
        fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #expression); \
        return 1; \
    } \
} while (0)

struct observer_state
{
    struct em8051_debug_event_bus *bus;
    uint64_t sequences[8];
    unsigned calls;
    unsigned order_value;
    enum em8051_debug_event_status subscribe_status;
    enum em8051_debug_event_status unsubscribe_status;
    enum em8051_debug_event_status emit_status;
};

static void ordering_observer(const struct em8051_debug_event *aEvent, void *aUser)
{
    struct observer_state *state = (struct observer_state *)aUser;
    state->sequences[state->calls++] = aEvent->sequence;
    state->order_value = state->order_value * 10u + 1u;
}

static void second_observer(const struct em8051_debug_event *aEvent, void *aUser)
{
    struct observer_state *state = (struct observer_state *)aUser;
    (void)aEvent;
    state->order_value = state->order_value * 10u + 2u;
}

static void noop_observer(const struct em8051_debug_event *aEvent, void *aUser)
{
    (void)aEvent;
    (void)aUser;
}

static void hostile_observer(const struct em8051_debug_event *aEvent,
                             void *aUser)
{
    struct em8051_debug_event *mutable_event =
        (struct em8051_debug_event *)(uintptr_t)aEvent;
    (void)aUser;
    mutable_event->sequence = 999u;
}

static void reentrant_observer(const struct em8051_debug_event *aEvent, void *aUser)
{
    struct observer_state *state = (struct observer_state *)aUser;
    uint32_t id = 0u;
    state->subscribe_status = em8051_debug_event_bus_subscribe(
        state->bus, noop_observer, NULL, &id);
    state->unsubscribe_status = em8051_debug_event_bus_unsubscribe(state->bus, 1u);
    state->emit_status = em8051_debug_event_bus_emit(state->bus, aEvent, NULL);
}

static struct em8051_debug_value value(uint64_t aValue, unsigned aWidth,
                                       bool aSigned, bool aKnown)
{
    struct em8051_debug_value result;
    memset(&result, 0, sizeof(result));
    result.value = aValue;
    result.width = (uint8_t)aWidth;
    result.is_signed = aSigned ? 1u : 0u;
    result.known = aKnown ? 1u : 0u;
    return result;
}

static struct em8051_debug_event memory_event(uint64_t aSequence,
                                               uint64_t aOld,
                                               uint64_t aNew)
{
    struct em8051_debug_event event;
    memset(&event, 0, sizeof(event));
    event.sequence = aSequence;
    event.kind = EM8051_DEBUG_EVENT_MEMORY_WRITE;
    event.address_space = EM8051_DEBUG_SPACE_XDATA;
    event.address = 0x1234u;
    event.access = EM8051_DEBUG_ACCESS_WRITE;
    event.old_value = value(aOld, 8u, false, true);
    event.new_value = value(aNew, 8u, false, true);
    return event;
}

static struct em8051_debug_watch_action action(
    enum em8051_debug_watch_action_kind aKind,
    enum em8051_debug_watch_operand aOperand,
    enum em8051_debug_watch_compare aComparison, uint64_t aConstant)
{
    struct em8051_debug_watch_action result;
    memset(&result, 0, sizeof(result));
    result.kind = (uint8_t)aKind;
    result.condition.present = 1u;
    result.condition.operand = (uint8_t)aOperand;
    result.condition.comparison = (uint8_t)aComparison;
    result.condition.width = 8u;
    result.condition.constant = aConstant;
    return result;
}

static struct em8051_debug_watch base_watch(uint32_t aId)
{
    struct em8051_debug_watch result;
    memset(&result, 0, sizeof(result));
    result.id = aId;
    result.enabled = 1u;
    result.address_space = EM8051_DEBUG_SPACE_XDATA;
    result.address_first = 0x1234u;
    result.address_last = 0x1234u;
    result.access_mask = EM8051_DEBUG_ACCESS_WRITE;
    return result;
}

static int test_event_bus(void)
{
    struct em8051_debug_event_bus bus;
    struct observer_state first;
    struct observer_state reentrant;
    struct em8051_debug_event event;
    uint32_t ids[EM8051_DEBUG_EVENT_MAX_OBSERVERS];
    uint64_t sequence = 0u;
    size_t i;

    memset(&first, 0, sizeof(first));
    memset(&reentrant, 0, sizeof(reentrant));
    memset(&event, 0, sizeof(event));
    em8051_debug_event_bus_init(&bus);
    reentrant.bus = &bus;
    CHECK(bus.next_sequence == 1u);
    CHECK(em8051_debug_event_bus_subscribe(&bus, ordering_observer, &first,
                                           &ids[0]) == EM8051_DEBUG_EVENT_OK);
    CHECK(em8051_debug_event_bus_subscribe(&bus, second_observer, &first,
                                           &ids[1]) == EM8051_DEBUG_EVENT_OK);
    CHECK(em8051_debug_event_bus_subscribe(&bus, reentrant_observer, &reentrant,
                                           &ids[2]) == EM8051_DEBUG_EVENT_OK);
    CHECK(em8051_debug_event_bus_emit(&bus, &event, &sequence) ==
          EM8051_DEBUG_EVENT_OK);
    CHECK(sequence == 1u && first.sequences[0] == 1u && first.order_value == 12u);
    CHECK(event.sequence == 0u); /* caller's immutable source was not changed */
    CHECK(reentrant.subscribe_status == EM8051_DEBUG_EVENT_BUSY);
    CHECK(reentrant.unsubscribe_status == EM8051_DEBUG_EVENT_BUSY);
    CHECK(reentrant.emit_status == EM8051_DEBUG_EVENT_BUSY);
    CHECK(em8051_debug_event_bus_unsubscribe(&bus, ids[1]) ==
          EM8051_DEBUG_EVENT_OK);
    first.order_value = 0u;
    CHECK(em8051_debug_event_bus_emit(&bus, &event, &sequence) ==
          EM8051_DEBUG_EVENT_OK);
    CHECK(sequence == 2u && first.sequences[1] == 2u && first.order_value == 1u);

    em8051_debug_event_bus_init(&bus);
    memset(&first, 0, sizeof(first));
    CHECK(em8051_debug_event_bus_subscribe(&bus, hostile_observer, NULL,
                                           &ids[0]) == EM8051_DEBUG_EVENT_OK);
    CHECK(em8051_debug_event_bus_subscribe(&bus, ordering_observer, &first,
                                           &ids[1]) == EM8051_DEBUG_EVENT_OK);
    CHECK(em8051_debug_event_bus_emit(&bus, &event, &sequence) ==
          EM8051_DEBUG_EVENT_OK);
    CHECK(sequence == 1u && first.sequences[0] == 1u);

    em8051_debug_event_bus_init(&bus);
    for (i = 0u; i < EM8051_DEBUG_EVENT_MAX_OBSERVERS; ++i)
        CHECK(em8051_debug_event_bus_subscribe(&bus, noop_observer, NULL,
                                               &ids[i]) == EM8051_DEBUG_EVENT_OK);
    CHECK(em8051_debug_event_bus_subscribe(&bus, noop_observer, NULL,
                                           &ids[0]) == EM8051_DEBUG_EVENT_LIMIT);
    return 0;
}

static int test_comparisons(void)
{
    const enum em8051_debug_watch_compare operators[] = {
        EM8051_DEBUG_WATCH_EQ, EM8051_DEBUG_WATCH_NE,
        EM8051_DEBUG_WATCH_LT, EM8051_DEBUG_WATCH_LE,
        EM8051_DEBUG_WATCH_GT, EM8051_DEBUG_WATCH_GE
    };
    const unsigned expected[] = {0u, 1u, 1u, 1u, 0u, 0u};
    struct em8051_debug_watch_table table;
    struct em8051_debug_watch watch;
    struct em8051_debug_watch_result result;
    struct em8051_debug_event event = memory_event(44u, 9u, 10u);
    size_t required;
    size_t i;

    em8051_debug_watch_table_init(&table);
    for (i = 0u; i < sizeof(operators) / sizeof(operators[0]); ++i)
    {
        watch = base_watch(9u);
        watch.action_count = 1u;
        watch.actions[0] = action(EM8051_DEBUG_WATCH_STOP,
                                  EM8051_DEBUG_WATCH_OLD,
                                  operators[i], 10u);
        CHECK(em8051_debug_watch_table_replace(&table, &watch, 1u) ==
              EM8051_DEBUG_EVENT_OK);
        CHECK(em8051_debug_watch_table_match(&table, &event, &result, 1u,
                                             &required) == EM8051_DEBUG_EVENT_OK);
        CHECK(required == expected[i]);
    }
    return 0;
}

static int test_signed_edges_and_unknown(void)
{
    struct em8051_debug_watch_table table;
    struct em8051_debug_watch watch = base_watch(1u);
    struct em8051_debug_watch_result result;
    struct em8051_debug_event event = memory_event(1u, 0u, 0u);
    size_t required;

    watch.action_count = 1u;
    watch.actions[0] = action(EM8051_DEBUG_WATCH_STOP,
                              EM8051_DEBUG_WATCH_NEW,
                              EM8051_DEBUG_WATCH_LT, UINT64_MAX);
    watch.actions[0].condition.is_signed = 1u;
    event.new_value = value(UINT64_C(0x80), 8u, true, true);
    em8051_debug_watch_table_init(&table);
    CHECK(em8051_debug_watch_table_replace(&table, &watch, 1u) ==
          EM8051_DEBUG_EVENT_OK);
    CHECK(em8051_debug_watch_table_match(&table, &event, &result, 1u,
                                         &required) == EM8051_DEBUG_EVENT_OK);
    CHECK(required == 1u); /* -128 < -1 */
    event.new_value.known = 0u;
    CHECK(em8051_debug_watch_table_match(&table, &event, &result, 1u,
                                         &required) == EM8051_DEBUG_EVENT_OK);
    CHECK(required == 0u);
    event.new_value = value(0xffu, 8u, false, true);
    CHECK(em8051_debug_watch_table_match(&table, &event, &result, 1u,
                                         &required) == EM8051_DEBUG_EVENT_OK);
    CHECK(required == 0u); /* signedness mismatch is false */

    watch.actions[0].condition.width = 64u;
    watch.actions[0].condition.constant = UINT64_MAX;
    event.new_value = value(UINT64_C(0x8000000000000000), 64u, true, true);
    CHECK(em8051_debug_watch_table_replace(&table, &watch, 1u) ==
          EM8051_DEBUG_EVENT_OK);
    CHECK(em8051_debug_watch_table_match(&table, &event, &result, 1u,
                                         &required) == EM8051_DEBUG_EVENT_OK);
    CHECK(required == 1u); /* INT64_MIN < -1, without implementation casts */

    watch.actions[0].condition.width = 8u;
    watch.actions[0].condition.is_signed = 0u;
    watch.actions[0].condition.constant = 256u;
    CHECK(em8051_debug_watch_table_replace(&table, &watch, 1u) ==
          EM8051_DEBUG_EVENT_INVALID_ARGUMENT);
    return 0;
}

static int test_actions_order_bounds_and_atomicity(void)
{
    struct em8051_debug_watch_table table;
    struct em8051_debug_watch watches[2];
    struct em8051_debug_watch invalid;
    struct em8051_debug_watch_result results[2];
    struct em8051_debug_event event = memory_event(77u, 100u, 201u);
    size_t required;

    watches[0] = base_watch(20u);
    watches[0].action_count = 5u;
    watches[0].actions[0] = action(EM8051_DEBUG_WATCH_STOP,
                                   EM8051_DEBUG_WATCH_NEW,
                                   EM8051_DEBUG_WATCH_GE, 200u);
    watches[0].actions[1] = action(EM8051_DEBUG_WATCH_STOP,
                                   EM8051_DEBUG_WATCH_NEW,
                                   EM8051_DEBUG_WATCH_GT, 180u);
    watches[0].actions[2] = action(EM8051_DEBUG_WATCH_CONSOLE,
                                   EM8051_DEBUG_WATCH_NEW,
                                   EM8051_DEBUG_WATCH_GT, 180u);
    watches[0].actions[3] = action(EM8051_DEBUG_WATCH_QUIET,
                                   EM8051_DEBUG_WATCH_OLD,
                                   EM8051_DEBUG_WATCH_EQ, 100u);
    memset(&watches[0].actions[4], 0, sizeof(watches[0].actions[4]));
    watches[0].actions[4].kind = EM8051_DEBUG_WATCH_ROUTE;
    watches[0].actions[4].trace_id_count = 2u;
    watches[0].actions[4].trace_ids[0] = 7u;
    watches[0].actions[4].trace_ids[1] = 23u;
    watches[1] = base_watch(3u);
    watches[1].action_count = 1u;
    watches[1].actions[0] = action(EM8051_DEBUG_WATCH_CONSOLE,
                                   EM8051_DEBUG_WATCH_NEW,
                                   EM8051_DEBUG_WATCH_EQ, 201u);
    em8051_debug_watch_table_init(&table);
    CHECK(em8051_debug_watch_table_replace(&table, watches, 2u) ==
          EM8051_DEBUG_EVENT_OK);
    CHECK(table.watches[0].id == 3u && table.watches[1].id == 20u);
    CHECK(em8051_debug_watch_table_match(&table, &event, results, 2u,
                                         &required) == EM8051_DEBUG_EVENT_OK);
    CHECK(required == 2u && results[0].watch_id == 3u &&
          results[1].watch_id == 20u);
    CHECK(results[1].stop == 1u); /* two stop actions coalesced */
    CHECK(results[1].quiet == 1u && results[1].console == 0u);
    CHECK(results[1].trace_id_count == 2u && results[1].trace_ids[0] == 7u &&
          results[1].trace_ids[1] == 23u);
    CHECK(results[1].source_event_sequence == 77u);
    CHECK(results[1].fired_action_mask == 0x1fu);
    CHECK(em8051_debug_watch_table_match(&table, &event, results, 1u,
                                         &required) == EM8051_DEBUG_EVENT_LIMIT);
    CHECK(required == 2u);

    invalid = base_watch(3u);
    invalid.action_count = 1u;
    invalid.actions[0] = action(EM8051_DEBUG_WATCH_STOP,
                                EM8051_DEBUG_WATCH_NEW,
                                EM8051_DEBUG_WATCH_EQ, 1u);
    watches[0] = invalid;
    watches[1] = invalid;
    CHECK(em8051_debug_watch_table_replace(&table, watches, 2u) ==
          EM8051_DEBUG_EVENT_DUPLICATE_ID);
    CHECK(table.watch_count == 2u && table.watches[0].id == 3u); /* atomic */

    event.address = 0x9999u;
    CHECK(em8051_debug_watch_table_match(&table, &event, results, 2u,
                                         &required) == EM8051_DEBUG_EVENT_OK);
    CHECK(required == 0u);
    event.address = 0x1234u;
    event.kind = EM8051_DEBUG_EVENT_WATCH_MATCH;
    CHECK(em8051_debug_watch_table_match(&table, &event, results, 2u,
                                         &required) == EM8051_DEBUG_EVENT_OK);
    CHECK(required == 0u); /* derived matches cannot recursively match */
    event.address = 0x1234u;
    event.kind = EM8051_DEBUG_EVENT_WATCH_MATCH;
    CHECK(em8051_debug_watch_table_match(&table, &event, results, 2u,
                                         &required) == EM8051_DEBUG_EVENT_OK);
    CHECK(required == 0u); /* derived watch events cannot recursively match */
    return 0;
}

int main(void)
{
    CHECK(test_event_bus() == 0);
    CHECK(test_comparisons() == 0);
    CHECK(test_signed_edges_and_unknown() == 0);
    CHECK(test_actions_order_bounds_and_atomicity() == 0);
    puts("debug event/watch tests passed");
    return 0;
}
