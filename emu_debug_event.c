/* Generic deterministic debugger event and watch matching runtime.
 * Copyright 2026 Hans-Einar
 *
 * Distributed under the MIT license; see LICENSE.
 */

#include "emu_debug_event.h"

#include <limits.h>
#include <string.h>

static bool value_width_valid(uint8_t aWidth)
{
    return aWidth == 8u || aWidth == 16u || aWidth == 32u || aWidth == 64u;
}

static uint64_t width_mask(uint8_t aWidth)
{
    if (aWidth == 64u)
        return UINT64_MAX;
    return (UINT64_C(1) << aWidth) - UINT64_C(1);
}

static bool constant_valid(const struct em8051_debug_watch_condition *aCondition)
{
    uint64_t mask;
    uint64_t sign;

    if (!value_width_valid(aCondition->width))
        return false;
    mask = width_mask(aCondition->width);
    if (aCondition->is_signed != 0u)
    {
        if (aCondition->width == 64u)
            return true;
        sign = UINT64_C(1) << (aCondition->width - 1u);
        if ((aCondition->constant & sign) != 0u)
            return (aCondition->constant | mask) == UINT64_MAX;
        return (aCondition->constant & ~mask) == 0u;
    }
    return (aCondition->constant & ~mask) == 0u;
}

static bool compare_value(const struct em8051_debug_value *aValue,
                          const struct em8051_debug_watch_condition *aCondition)
{
    uint64_t left;
    uint64_t right;
    int ordering;

    if (aValue->known == 0u || aValue->width != aCondition->width ||
        aValue->is_signed != aCondition->is_signed)
        return false;

    left = aValue->value & width_mask(aValue->width);
    right = aCondition->constant & width_mask(aCondition->width);
    if (aCondition->is_signed != 0u)
    {
        uint64_t sign = UINT64_C(1) << (aValue->width - 1u);
        bool left_negative = (left & sign) != 0u;
        bool right_negative = (right & sign) != 0u;

        if (left_negative != right_negative)
            ordering = left_negative ? -1 : 1;
        else
            ordering = left < right ? -1 : left > right;
    }
    else
        ordering = left < right ? -1 : left > right;

    switch ((enum em8051_debug_watch_compare)aCondition->comparison)
    {
    case EM8051_DEBUG_WATCH_EQ: return ordering == 0;
    case EM8051_DEBUG_WATCH_NE: return ordering != 0;
    case EM8051_DEBUG_WATCH_LT: return ordering < 0;
    case EM8051_DEBUG_WATCH_LE: return ordering <= 0;
    case EM8051_DEBUG_WATCH_GT: return ordering > 0;
    case EM8051_DEBUG_WATCH_GE: return ordering >= 0;
    }
    return false;
}

static bool condition_matches(const struct em8051_debug_watch_condition *aCondition,
                              const struct em8051_debug_event *aEvent)
{
    if (aCondition->present == 0u)
        return true;
    if (aCondition->operand == EM8051_DEBUG_WATCH_OLD)
        return compare_value(&aEvent->old_value, aCondition);
    return compare_value(&aEvent->new_value, aCondition);
}

static bool action_valid(const struct em8051_debug_watch_action *aAction)
{
    size_t i;

    if (aAction->kind > EM8051_DEBUG_WATCH_ROUTE)
        return false;
    if (aAction->condition.present > 1u)
        return false;
    if (aAction->condition.present != 0u)
    {
        if (aAction->condition.operand > EM8051_DEBUG_WATCH_NEW ||
            aAction->condition.comparison > EM8051_DEBUG_WATCH_GE ||
            aAction->condition.is_signed > 1u ||
            !constant_valid(&aAction->condition))
            return false;
    }
    if (aAction->kind != EM8051_DEBUG_WATCH_ROUTE)
        return aAction->trace_id_count == 0u;
    if (aAction->trace_id_count == 0u ||
        aAction->trace_id_count > EM8051_DEBUG_WATCH_MAX_ROUTES)
        return false;
    for (i = 0u; i < aAction->trace_id_count; ++i)
    {
        if (aAction->trace_ids[i] == 0u ||
            (i != 0u && aAction->trace_ids[i - 1u] >= aAction->trace_ids[i]))
            return false;
    }
    return true;
}

static bool watch_valid(const struct em8051_debug_watch *aWatch)
{
    uint32_t routes[EM8051_DEBUG_WATCH_MAX_ROUTES];
    size_t route_count = 0u;
    size_t i;

    if (aWatch->id == 0u || aWatch->enabled > 1u ||
        aWatch->address_space == EM8051_DEBUG_SPACE_NONE ||
        aWatch->address_space > EM8051_DEBUG_SPACE_XDATA ||
        aWatch->address_first > aWatch->address_last ||
        aWatch->access_mask == 0u ||
        (aWatch->access_mask & ~(EM8051_DEBUG_ACCESS_FETCH |
                                 EM8051_DEBUG_ACCESS_READ |
                                 EM8051_DEBUG_ACCESS_WRITE |
                                 EM8051_DEBUG_ACCESS_RMW)) != 0u ||
        aWatch->action_count == 0u ||
        aWatch->action_count > EM8051_DEBUG_WATCH_MAX_ACTIONS)
        return false;
    for (i = 0u; i < aWatch->action_count; ++i)
    {
        size_t route_index;
        if (!action_valid(&aWatch->actions[i]))
            return false;
        if (aWatch->actions[i].kind != EM8051_DEBUG_WATCH_ROUTE)
            continue;
        for (route_index = 0u;
             route_index < aWatch->actions[i].trace_id_count; ++route_index)
        {
            uint32_t id = aWatch->actions[i].trace_ids[route_index];
            size_t existing;
            for (existing = 0u; existing < route_count; ++existing)
                if (routes[existing] == id)
                    break;
            if (existing != route_count)
                continue;
            if (route_count == EM8051_DEBUG_WATCH_MAX_ROUTES)
                return false;
            routes[route_count++] = id;
        }
    }
    return true;
}

static void insert_route(struct em8051_debug_watch_result *aResult,
                         uint32_t aTraceId)
{
    size_t at = 0u;
    size_t i;

    while (at < aResult->trace_id_count && aResult->trace_ids[at] < aTraceId)
        ++at;
    if (at < aResult->trace_id_count && aResult->trace_ids[at] == aTraceId)
        return;
    for (i = aResult->trace_id_count; i > at; --i)
        aResult->trace_ids[i] = aResult->trace_ids[i - 1u];
    aResult->trace_ids[at] = aTraceId;
    ++aResult->trace_id_count;
}

void em8051_debug_event_bus_init(struct em8051_debug_event_bus *aBus)
{
    if (aBus == NULL)
        return;
    memset(aBus, 0, sizeof(*aBus));
    aBus->next_sequence = 1u;
    aBus->next_observer_id = 1u;
}

enum em8051_debug_event_status em8051_debug_event_bus_subscribe(
    struct em8051_debug_event_bus *aBus,
    em8051_debug_event_observer aCallback, void *aUser,
    uint32_t *aObserverId)
{
    struct em8051_debug_event_observer_slot *slot;

    if (aBus == NULL || aCallback == NULL || aObserverId == NULL)
        return EM8051_DEBUG_EVENT_INVALID_ARGUMENT;
    if (aBus->dispatching != 0u)
        return EM8051_DEBUG_EVENT_BUSY;
    if (aBus->observer_count >= EM8051_DEBUG_EVENT_MAX_OBSERVERS ||
        aBus->next_observer_id == 0u)
        return EM8051_DEBUG_EVENT_LIMIT;
    slot = &aBus->observers[aBus->observer_count++];
    slot->callback = aCallback;
    slot->user = aUser;
    slot->id = aBus->next_observer_id++;
    *aObserverId = slot->id;
    return EM8051_DEBUG_EVENT_OK;
}

enum em8051_debug_event_status em8051_debug_event_bus_unsubscribe(
    struct em8051_debug_event_bus *aBus, uint32_t aObserverId)
{
    size_t i;

    if (aBus == NULL || aObserverId == 0u)
        return EM8051_DEBUG_EVENT_INVALID_ARGUMENT;
    if (aBus->dispatching != 0u)
        return EM8051_DEBUG_EVENT_BUSY;
    for (i = 0u; i < aBus->observer_count; ++i)
    {
        if (aBus->observers[i].id == aObserverId)
        {
            memmove(&aBus->observers[i], &aBus->observers[i + 1u],
                    (aBus->observer_count - i - 1u) * sizeof(aBus->observers[0]));
            --aBus->observer_count;
            return EM8051_DEBUG_EVENT_OK;
        }
    }
    return EM8051_DEBUG_EVENT_INVALID_ARGUMENT;
}

enum em8051_debug_event_status em8051_debug_event_bus_emit(
    struct em8051_debug_event_bus *aBus,
    const struct em8051_debug_event *aEvent,
    uint64_t *aAssignedSequence)
{
    struct em8051_debug_event event;
    size_t i;

    if (aBus == NULL || aEvent == NULL)
        return EM8051_DEBUG_EVENT_INVALID_ARGUMENT;
    if (aBus->dispatching != 0u)
        return EM8051_DEBUG_EVENT_BUSY;
    if (aBus->next_sequence == 0u || aEvent->sequence != 0u)
        return EM8051_DEBUG_EVENT_INVALID_ARGUMENT;
    event = *aEvent;
    event.sequence = aBus->next_sequence++;
    aBus->dispatching = 1u;
    for (i = 0u; i < aBus->observer_count; ++i)
    {
        struct em8051_debug_event observer_event = event;
        aBus->observers[i].callback(&observer_event,
                                    aBus->observers[i].user);
    }
    aBus->dispatching = 0u;
    if (aAssignedSequence != NULL)
        *aAssignedSequence = event.sequence;
    return EM8051_DEBUG_EVENT_OK;
}

void em8051_debug_watch_table_init(struct em8051_debug_watch_table *aTable)
{
    if (aTable != NULL)
        memset(aTable, 0, sizeof(*aTable));
}

enum em8051_debug_event_status em8051_debug_watch_table_replace(
    struct em8051_debug_watch_table *aTable,
    const struct em8051_debug_watch *aWatches, size_t aCount)
{
    struct em8051_debug_watch sorted[EM8051_DEBUG_WATCH_MAX_WATCHES];
    size_t i;

    if (aTable == NULL || (aCount != 0u && aWatches == NULL))
        return EM8051_DEBUG_EVENT_INVALID_ARGUMENT;
    if (aCount > EM8051_DEBUG_WATCH_MAX_WATCHES)
        return EM8051_DEBUG_EVENT_LIMIT;
    for (i = 0u; i < aCount; ++i)
    {
        size_t at = i;
        if (!watch_valid(&aWatches[i]))
            return EM8051_DEBUG_EVENT_INVALID_ARGUMENT;
        while (at != 0u && sorted[at - 1u].id > aWatches[i].id)
        {
            sorted[at] = sorted[at - 1u];
            --at;
        }
        sorted[at] = aWatches[i];
    }
    for (i = 1u; i < aCount; ++i)
        if (sorted[i - 1u].id == sorted[i].id)
            return EM8051_DEBUG_EVENT_DUPLICATE_ID;
    if (aCount != 0u)
        memcpy(aTable->watches, sorted, aCount * sizeof(sorted[0]));
    aTable->watch_count = (uint8_t)aCount;
    return EM8051_DEBUG_EVENT_OK;
}

enum em8051_debug_event_status em8051_debug_watch_table_match(
    const struct em8051_debug_watch_table *aTable,
    const struct em8051_debug_event *aEvent,
    struct em8051_debug_watch_result *aResults, size_t aCapacity,
    size_t *aRequired)
{
    size_t result_count = 0u;
    size_t i;

    if (aTable == NULL || aEvent == NULL || aRequired == NULL ||
        (aCapacity != 0u && aResults == NULL))
        return EM8051_DEBUG_EVENT_INVALID_ARGUMENT;
    if (aEvent->kind == EM8051_DEBUG_EVENT_WATCH_MATCH)
    {
        *aRequired = 0u;
        return EM8051_DEBUG_EVENT_OK;
    }
    if (aEvent->kind == EM8051_DEBUG_EVENT_WATCH_MATCH)
    {
        *aRequired = 0u;
        return EM8051_DEBUG_EVENT_OK;
    }
    for (i = 0u; i < aTable->watch_count; ++i)
    {
        const struct em8051_debug_watch *watch = &aTable->watches[i];
        struct em8051_debug_watch_result result;
        size_t action_index;

        if (watch->enabled == 0u || watch->address_space != aEvent->address_space ||
            aEvent->address < watch->address_first ||
            aEvent->address > watch->address_last ||
            (watch->access_mask & aEvent->access) == 0u)
            continue;
        memset(&result, 0, sizeof(result));
        result.watch_id = watch->id;
        result.source_event_sequence = aEvent->sequence;
        for (action_index = 0u; action_index < watch->action_count; ++action_index)
        {
            const struct em8051_debug_watch_action *action =
                &watch->actions[action_index];
            size_t route_index;

            if (!condition_matches(&action->condition, aEvent))
                continue;
            result.fired_action_mask |= UINT32_C(1) << action_index;
            if (action->kind == EM8051_DEBUG_WATCH_STOP)
                result.stop = 1u;
            else if (action->kind == EM8051_DEBUG_WATCH_CONSOLE)
                result.console = 1u;
            else if (action->kind == EM8051_DEBUG_WATCH_QUIET)
                result.quiet = 1u;
            else
                for (route_index = 0u; route_index < action->trace_id_count;
                     ++route_index)
                    insert_route(&result, action->trace_ids[route_index]);
        }
        if (result.fired_action_mask == 0u)
            continue;
        if (result.quiet != 0u)
            result.console = 0u;
        if (result_count < aCapacity)
            aResults[result_count] = result;
        ++result_count;
    }
    *aRequired = result_count;
    return result_count > aCapacity ? EM8051_DEBUG_EVENT_LIMIT :
                                      EM8051_DEBUG_EVENT_OK;
}
