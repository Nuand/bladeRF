/*
 * This file is part of the bladeRF project:
 *   http://www.github.com/nuand/bladeRF
 *
 * Copyright (C) 2014 Nuand LLC
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */
#include "test_ctrl.h"
#include <string.h>

DECLARE_TEST_CASE(gain);

#define MAX_STAGES 8

#define __round_int(x) (x >= 0 ? (int)(x + 0.5) : (int)(x - 0.5))

struct gain {
    char const *name;
    float min;
    float max;
    float inc;
};

static inline int set_and_check(struct bladerf *dev,
                                bladerf_channel ch,
                                struct gain const *g,
                                int gain)
{
    int readback;
    int status;

    if (0 == strcmp(g->name, "overall")) {
        status = bladerf_set_gain(dev, ch, gain);
        if (status != 0) {
            PR_ERROR("Failed to set %s gain: %s\n", g->name,
                     bladerf_strerror(status));
            return status;
        }

        status = bladerf_get_gain(dev, ch, &readback);
        if (status != 0) {
            PR_ERROR("Failed to read back %s gain: %s\n", g->name,
                     bladerf_strerror(status));
            return status;
        }
    } else {
        status = bladerf_set_gain_stage(dev, ch, g->name, gain);
        if (status != 0) {
            PR_ERROR("Failed to set %s gain: %s\n", g->name,
                     bladerf_strerror(status));
            return status;
        }

        status = bladerf_get_gain_stage(dev, ch, g->name, &readback);
        if (status != 0) {
            PR_ERROR("Failed to read back %s gain: %s\n", g->name,
                     bladerf_strerror(status));
            return status;
        }
    }
    if (gain != readback) {
        PR_ERROR("Erroneous %s gain readback=%d, expected=%d\n", g->name,
                 readback, gain);
        return -1;
    }

    return 0;
}

static failure_count gain_sweep(struct bladerf *dev,
                                bladerf_channel ch,
                                bool quiet)
{
    char const **stages    = NULL;
    failure_count failures = 0;
    size_t i;
    int status;

    stages = calloc(MAX_STAGES + 1, sizeof(char *));
    if (NULL == stages) {
        PR_ERROR("Failed to calloc gain stage array\n");
        return -1;
    }

    int const n_stages = bladerf_get_gain_stages(dev, ch, stages, MAX_STAGES);
    if (n_stages < 0) {
        PR_ERROR("Failed to enumerate gain stages: %s\n",
                 bladerf_strerror(n_stages));
        failures = -1;
        goto out;
    }

    for (i = 0; i <= (unsigned)n_stages; i++) {
        struct bladerf_range const *range;
        struct gain stage;
        float gain;

        if ((unsigned)n_stages == i) {
            // magic null value
            stage.name = "overall";

            PRINT("    %s\n", stage.name);

            status = bladerf_get_gain_range(dev, ch, &range);
            if (status < 0) {
                PR_ERROR("Failed to retrieve range of overall gain: %s\n",
                         bladerf_strerror(n_stages));
                failures++;
                continue;
            }
        } else {
            stage.name = stages[i];

            PRINT("    %s\n", stage.name);

            status = bladerf_get_gain_stage_range(dev, ch, stage.name, &range);
            if (status < 0) {
                PR_ERROR("Failed to retrieve range of gain stage %s: %s\n",
                         stage.name, bladerf_strerror(n_stages));
                failures++;
                continue;
            }
        }

        stage.min = (float)(range->min * range->scale);
        stage.max = (float)(range->max * range->scale);
        stage.inc = (float)range->step;

        fflush(stdout);
        for (gain = stage.min; gain <= stage.max; gain += stage.inc) {
            status = set_and_check(dev, ch, &stage, __round_int(gain));
            if (status != 0) {
                failures++;
            }
        }
    }

out:
    free((char **)stages);

    return failures;
}

static failure_count random_gains(struct bladerf *dev,
                                  struct app_params *p,
                                  bladerf_channel ch,
                                  bool quiet)
{
    size_t const iterations = 250;

    char const **stages    = NULL;
    failure_count failures = 0;
    size_t i, j;
    int status;

    stages = calloc(MAX_STAGES + 1, sizeof(char *));
    if (NULL == stages) {
        PR_ERROR("Failed to calloc gain stage array\n");
        return -1;
    }

    int const n_stages = bladerf_get_gain_stages(dev, ch, stages, MAX_STAGES);
    if (n_stages < 0) {
        PR_ERROR("Failed to enumerate gain stages: %s\n",
                 bladerf_strerror(n_stages));
        failures = -1;
        goto out;
    }

    for (i = 0; i <= (unsigned)n_stages; i++) {
        struct bladerf_range const *range;
        struct gain stage;

        if ((unsigned)n_stages == i) {
            // magic null value
            stage.name = "overall";

            PRINT("    %s\n", stage.name);

            status = bladerf_get_gain_range(dev, ch, &range);
            if (status < 0) {
                PR_ERROR("Failed to retrieve range of overall gain: %s\n",
                         bladerf_strerror(n_stages));
                failures++;
                continue;
            }
        } else {
            stage.name = stages[i];

            PRINT("    %s\n", stage.name);

            status = bladerf_get_gain_stage_range(dev, ch, stage.name, &range);
            if (status < 0) {
                PR_ERROR("Failed to retrieve range of gain stage %s: %s\n",
                         stage.name, bladerf_strerror(n_stages));
                failures++;
                continue;
            }
        }

        stage.min = (range->min * range->scale);
        stage.max = (range->max * range->scale);
        stage.inc = (range->step * range->scale);

        int const n_incs = __round_int((stage.max - stage.min) / stage.inc);

        for (j = 0; j < iterations; j++) {
            float gain;

            randval_update(&p->randval_state);
            gain = stage.min + (p->randval_state % n_incs) * stage.inc;

            if (gain > stage.max) {
                gain = stage.max;
            } else if (gain < stage.min) {
                gain = stage.min;
            }

            status = set_and_check(dev, ch, &stage, __round_int(gain));
            if (status != 0) {
                failures++;
            }
        }
    }

out:
    free((char **)stages);

    return failures;
}

failure_count test_gain(struct bladerf *dev, struct app_params *p, bool quiet)
{
    failure_count failures = 0;

    bladerf_direction dir;

    bladerf_frequency const TEST_FREQ = 420000000;

    FOR_EACH_DIRECTION(dir)
    {
        size_t i;
        bladerf_channel ch;

        FOR_EACH_CHANNEL(dir, bladerf_get_channel_count(dev, dir), i, ch)
        {
            bladerf_gain_mode old_mode;
            int status;

            /* NOTE: This is a workaround for a bug in fpga v0.10.2 on bladerf2,
             * where the frequency post-initialization is not valid. */
            status = bladerf_set_frequency(dev, ch, TEST_FREQ);
            if (status != 0) {
                PR_ERROR("Failed to set frequency %" BLADERF_PRIuFREQ
                         " Hz: %s\n",
                         TEST_FREQ, bladerf_strerror(status));
                return status;
            }

            if (!BLADERF_CHANNEL_IS_TX(ch)) {
                /* Switch to manual gain control */
                status = bladerf_get_gain_mode(dev, ch, &old_mode);
                if (status < 0 && status != BLADERF_ERR_UNSUPPORTED) {
                    PR_ERROR(
                        "Failed to get current gain mode on channel %s: %s\n",
                        channel2str(ch), bladerf_strerror(status));
                    failures += 1;
                    continue;
                }

                status = bladerf_set_gain_mode(dev, ch, BLADERF_GAIN_MGC);
                if (status < 0 && status != BLADERF_ERR_UNSUPPORTED) {
                    PR_ERROR("Failed to set gain mode on channel %s: %s\n",
                             channel2str(ch), bladerf_strerror(status));
                    failures += 1;
                    continue;
                }

                if (!p->module_enabled) {
                    /* Enable channel (necessary for RX gain changes to register
                     * on bladerf2) */
                    status = bladerf_enable_module(dev, ch, true);
                    if (status < 0) {
                        PR_ERROR("Failed to enable channel %s: %s\n",
                                 channel2str(ch), bladerf_strerror(status));
                        failures += 1;
                        continue;
                    }
                }
            }

            PRINT("%s: Performing gain sweep on %s...\n", __FUNCTION__,
                  channel2str(ch));
            failures += gain_sweep(dev, ch, quiet);

            PRINT("%s: Applying random gains on %s...\n", __FUNCTION__,
                  channel2str(ch));
            failures += random_gains(dev, p, ch, quiet);

            if (!BLADERF_CHANNEL_IS_TX(ch)) {
                if (!p->module_enabled) {
                    /* Deactivate the channel */
                    status = bladerf_enable_module(dev, ch, false);
                    if (status < 0) {
                        PR_ERROR("Failed to deactivate channel %s: %s\n",
                                 channel2str(ch), bladerf_strerror(status));
                        failures += 1;
                        continue;
                    }
                }

                /* Return to previous gain control mode */
                status = bladerf_set_gain_mode(dev, ch, old_mode);
                if (status < 0 && status != BLADERF_ERR_UNSUPPORTED) {
                    PR_ERROR("Failed to set gain mode on channel %s: %s\n",
                             channel2str(ch), bladerf_strerror(status));
                    failures += 1;
                    continue;
                }
            }
        }
    }

    return failures;
}
