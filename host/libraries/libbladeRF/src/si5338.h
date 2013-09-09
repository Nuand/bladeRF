#ifndef SI5338_H__
#define SI5338_H__

#include <libbladeRF.h>

// XXX document

int si5338_set_tx_freq(struct bladerf *dev, unsigned int freq);
int si5338_set_rx_freq(struct bladerf *dev, unsigned int freq);
int si5338_set_sample_rate(struct bladerf *dev, bladerf_module module, uint32_t rate, uint32_t *actual);
int si5338_get_sample_rate(struct bladerf *dev, bladerf_module module, unsigned int *rate);
int si5338_set_rational_sample_rate(struct bladerf *dev, bladerf_module module, struct bladerf_rational_rate *rate, struct bladerf_rational_rate *actual);
int si5338_get_rational_sample_rate(struct bladerf *dev, bladerf_module module, struct bladerf_rational_rate *rate);

#endif
