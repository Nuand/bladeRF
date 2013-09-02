#ifndef SI5338_H__
#define SI5338_H__

#include <libbladeRF.h>




int si5338_set_rational_sample_rate(struct bladerf *dev, bladerf_module module, uint32_t frequency, uint32_t numerator, uint32_t denom);

#endif
