/* Actual C++ consumer of the frozen table, linked to the runtime. MIT. */
#include <cassert>
#include <emusa80535/contracts/control3.h>
int main()
{
    emuif_control3_api api = {};
    api.struct_size = sizeof api;
    api.api_version = 1;
    assert(emusa80535_control_query_v3(&api) == EMUIF_OK);
    assert(api.open && api.request && api.close && api.advance);
    return 0;
}
