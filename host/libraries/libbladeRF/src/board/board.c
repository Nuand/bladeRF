#include "host_config.h"

#include "board.h"

extern const struct board_fns bladerf1_board_fns;
extern const struct board_fns bladerf2_board_fns;

const struct board_fns *bladerf_boards[] = {
    &bladerf1_board_fns,
    &bladerf2_board_fns,
};

const unsigned int bladerf_boards_len = ARRAY_SIZE(bladerf_boards);
