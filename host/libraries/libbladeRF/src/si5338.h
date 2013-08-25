#ifndef SI5338_H__
#define SI5338_H__

#include <libbladeRF.h>

// XXX document

int si5338_set_tx_freq(struct bladerf *dev, unsigned int freq);
int si5338_set_rx_freq(struct bladerf *dev, unsigned int freq);

#endif
