/* Compile-only SDK consumer: no target runtime implementation is linked. */
#include <stddef.h>
#include <emusa80535/contracts/debug.h>
#include <emusa80535/emu_debug.h>
#include <emusa80535/emu_debug_runtime.h>

typedef char prefix_size_at_zero[(offsetof(struct emuif_board_api, struct_size) == 0) ? 1 : -1];
typedef char prefix_api_at_four[(offsetof(struct emuif_board_api, api_version) == 4) ? 1 : -1];
typedef char prefix_flags_at_six[(offsetof(struct emuif_board_api, flags) == 6) ? 1 : -1];

int main(void)
{
    struct emuif_board_api board = {0};
    struct emuif_cpu_api cpu = {0};
    struct emuif_json_response response = {0};
    board.struct_size = (uint32_t)sizeof(board);
    board.api_version = EMUIF_API_VERSION;
    cpu.struct_size = (uint32_t)sizeof(cpu);
    cpu.api_version = EMUIF_API_VERSION;
    response.struct_size = (uint32_t)sizeof(response);
    response.api_version = EMUIF_API_VERSION;
    return EMUIF_TARGET_IMPLEMENTED || board.flags || cpu.flags || response.flags;
}
