/* Private wire3 cross-field validation. Copyright 2026 Hans-Einar. MIT.
 * Mirrors frozen target_wire.py request constraints before authorization. */
static bool wire3_boolean(const struct json_document *d, int a, const char *key)
{
    return wire3_literal(d, json_member(d, a, key), "true");
}
static bool wire3_ids(const struct json_document *d, int array, const char *key)
{
    int t;
    uint64_t previous = 0;
    for (t = array + 1; t < d->count && d->tokens[t].start < d->tokens[array].end;
         t = json_next(d, t))
    {
        uint64_t id = (uint64_t)number(d, t, key);
        if (id <= previous)
            return false;
        previous = id;
    }
    return true;
}
static bool wire3_has_id(const struct json_document *d, int array, const char *key, uint64_t id)
{
    int t;
    for (t = array + 1; t < d->count && d->tokens[t].start < d->tokens[array].end;
         t = json_next(d, t))
        if ((uint64_t)number(d, t, key) == id)
            return true;
    return false;
}
static bool wire3_routes(const struct json_document *d, int a, int traces)
{
    int t;
    uint64_t previous = 0;
    for (t = a + 1; t < d->count && d->tokens[t].start < d->tokens[a].end; t = json_next(d, t))
    {
        int64_t id;
        if (!json_integer(d, t, 1, UINT32_MAX, &id) || (uint64_t)id <= previous ||
            !wire3_has_id(d, traces, "traceId", (uint64_t)id))
            return false;
        previous = (uint64_t)id;
    }
    return true;
}
static bool wire3_config(const struct json_document *d, int config)
{
    static const char *categories[] = {"traces", "destinations", "points", "gates", "watches"};
    static const char *keys[] = {"traceId", "destinationId", "pointId", "gateId", "watchId"};
    int arrays[5], t, a;
    unsigned i;
    struct output compact = {0};
    json_compact(d, config, &compact);
    if (compact.failed || compact.length > 16384)
        return false;
    for (i = 0; i < 5; i++)
    {
        arrays[i] = json_member(d, config, categories[i]);
        if (!wire3_ids(d, arrays[i], keys[i]))
            return false;
    }
    a = arrays[0];
    for (t = a + 1; t < d->count && d->tokens[t].start < d->tokens[a].end; t = json_next(d, t))
        if (!wire3_has_id(d, arrays[1], "destinationId", (uint64_t)number(d, t, "destinationId")))
            return false;
    a = arrays[1];
    for (t = a + 1; t < d->count && d->tokens[t].start < d->tokens[a].end; t = json_next(d, t))
    {
        if (number(d, t, "capacity") < 2)
            return false;
        if (number(d, t, "kind") == 1 &&
            (number(d, t, "capacity") != 256 || wire3_boolean(d, t, "stopOnFull")))
            return false;
    }
    for (i = 2; i < 4; i++)
    {
        a = arrays[i];
        for (t = a + 1; t < d->count && d->tokens[t].start < d->tokens[a].end; t = json_next(d, t))
        {
            int sel = json_member(d, t, "selector");
            uint32_t mask = (uint32_t)number(d, sel, "matchMask");
            if (!wire3_routes(d, json_member(d, t, "traceIds"), arrays[0]))
                return false;
            if ((!(mask & 1) && number(d, sel, "kind")) || ((mask & 1) && !number(d, sel, "kind")))
                return false;
            if (!(mask & 2) && (number(d, sel, "pcFirst") || number(d, sel, "pcLast")))
                return false;
            if (!(mask & 4) && (number(d, sel, "addressFirst") || number(d, sel, "addressLast")))
                return false;
            if (!(mask & 8) && number(d, sel, "space"))
                return false;
            if (!(mask & 16) && number(d, sel, "accessMask"))
                return false;
            if (number(d, sel, "pcFirst") > number(d, sel, "pcLast") ||
                number(d, sel, "addressFirst") > number(d, sel, "addressLast"))
                return false;
        }
    }
    a = arrays[4];
    for (t = a + 1; t < d->count && d->tokens[t].start < d->tokens[a].end; t = json_next(d, t))
    {
        int actions = json_member(d, t, "actions"), action;
        if (number(d, t, "addressFirst") > number(d, t, "addressLast"))
            return false;
        for (action = actions + 1;
             action < d->count && d->tokens[action].start < d->tokens[actions].end;
             action = json_next(d, action))
        {
            int routes = json_member(d, action, "traceIds"),
                cond = json_member(d, action, "condition");
            uint64_t constant;
            uint32_t width = (uint32_t)number(d, cond, "width");
            if (!wire3_routes(d, routes, arrays[0]))
                return false;
            if (number(d, action, "kind") != 3 && d->tokens[routes].size)
                return false;
            if (!json_u64(d, json_member(d, cond, "constant"), &constant) ||
                (width < 64 && constant >= (UINT64_C(1) << width)))
                return false;
            if (!wire3_boolean(d, cond, "present") &&
                (number(d, cond, "operand") || number(d, cond, "comparison") || width != 8 ||
                 wire3_boolean(d, cond, "isSigned") || constant))
                return false;
        }
    }
    return true;
}
static bool wire3_semantics(const struct json_document *d, int a, const char *cmd)
{
    int t = json_member(d, a, "payload");
    if (t >= 0)
    {
        struct output compact = {0};
        json_compact(d, t, &compact);
        if (compact.failed || compact.length > 4096)
            return false;
    }
    if (!strcmp(cmd, "replaceTraceConfig"))
        return wire3_config(d, json_member(d, a, "config"));
    if (!strcmp(cmd, "captureStart"))
    {
        uint64_t context;
        int64_t mode = number(d, a, "mode");
        if (!json_u64(d, json_member(d, a, "contextId"), &context))
            return false;
        if (wire3_boolean(d, a, "rearmOnReset") && mode != 1)
            return false;
        if ((mode == 2) != (context > 0))
            return false;
        if (mode != 1 && number(d, a, "entryPc"))
            return false;
    }
    return true;
}
