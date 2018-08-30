#ifndef BLADERF1_FLASH_H_
#define BLADERF1_FLASH_H_

#include <libbladeRF.h>

#include <stdint.h>

/**
 * Write the provided data to the FX3 Firmware region to flash.
 *
 * This function does no validation of the data (i.e., that it's valid FW).
 *
 * @param       dev     bladeRF handle
 * @param[in]   image   Firmware image data
 * @param[in]   len     Length of firmware image data
 *
 * @return 0 on success, BLADERF_ERR_* value on failure
 */
int spi_flash_write_fx3_fw(struct bladerf *dev,
                           const uint8_t *image,
                           size_t len);

/**
 * Write the provided FPGA bitstream to flash and enable autoloading via
 * writing the associated metadata.
 *
 * @param       dev         bladeRF handle
 * @param[in]   bitstream   FPGA bitstream data
 * @param[in]   len         Length of the bitstream data
 *
 * @return 0 on success, BLADERF_ERR_* value on failure
 */
int spi_flash_write_fpga_bitstream(struct bladerf *dev,
                                   const uint8_t *bitstream,
                                   size_t len);

/**
 * Erase FPGA metadata and bitstream regions of flash.
 *
 * @param       dev             bladeRF handle
 *
 * @return 0 on success, BLADERF_ERR_* value on failure
 */
int spi_flash_erase_fpga(struct bladerf *dev);

/**
 * Read data from OTP ("otp") section of flash.
 *
 * @param       dev         Device handle
 * @param[in]   field       OTP field
 * @param[out]  data        Populated with retrieved data
 * @param[in]   data_size   Size of the data to read
 *
 * @return 0 on success, BLADERF_ERR_* on failure
 */
int spi_flash_read_otp(struct bladerf *dev,
                       char *field,
                       char *data,
                       size_t data_size);

/**
 * Read data from calibration ("cal") section of flash.
 *
 * @param       dev         Device handle
 * @param[in]   field       Calibration field
 * @param[out]  data        Populated with retrieved data
 * @param[in]   data_size   Size of the data to read
 *
 * @return 0 on success, BLADERF_ERR_* on failure
 */
int spi_flash_read_cal(struct bladerf *dev,
                       char *field,
                       char *data,
                       size_t data_size);

/**
 * Retrieve the device serial from flash.
 *
 * @pre The provided buffer is BLADERF_SERIAL_LENGTH in size
 *
 * @param       dev         Device handle. On success, serial field is updated
 * @param[out]  serial_buf  Populated with device serial
 *
 * @return 0 on success, BLADERF_ERR_* on failure
 */
int spi_flash_read_serial(struct bladerf *dev, char *serial_buf);

/**
 * Retrieve VCTCXO calibration value from flash.
 *
 * @param       dev         Device handle
 * @param[out]  dac_trim    DAC trim
 *
 * @return 0 on success, BLADERF_ERR_* on failure
 */
int spi_flash_read_vctcxo_trim(struct bladerf *dev, uint16_t *dac_trim);

/**
 * Retrieve FPGA size variant from flash.
 *
 * @param       dev         Device handle.
 * @param[out]  fpga_size   FPGA size
 *
 * @return 0 on success, BLADERF_ERR_* on failure
 */
int spi_flash_read_fpga_size(struct bladerf *dev, bladerf_fpga_size *fpga_size);

/**
 * Retrieve SPI flash manufacturer ID and device ID.
 *
 * @param       dev         Device handle.
 * @param[out]  mid         Flash manufacturer ID
 * @param[out]  did         Flash device ID
 *
 * @return 0 on success, BLADERF_ERR_* on failure
 */
int spi_flash_read_flash_id(struct bladerf *dev, uint8_t *mid, uint8_t *did);

/**
 * Decode SPI flash architecture given manufacturer and device IDs.
 *
 * @param       dev         Device handle.
 * @param       fpga_size   FPGA size
 *
 * @return 0 on success, BLADERF_ERR_* on failure
 */
int spi_flash_decode_flash_architecture(struct bladerf *dev,
                                        bladerf_fpga_size  *fpga_size);

/**
 * Encode a binary key-value pair.
 *
 * @param[in]       ptr     Pointer to data buffer that will contain encoded
 * data
 * @param[in]       len     Length of data buffer that will contain encoded data
 * @param[inout]    idx     Pointer indicating next free byte inside of data
 *                          buffer that will contain encoded data
 * @param[in]       field   Key of value to be stored in encoded data buffer
 * @param[in]       val     Value to be stored in encoded data buffer
 *
 * @return 0 on success, BLADERF_ERR_* on failure
 */
int binkv_encode_field(
    char *ptr, int len, int *idx, const char *field, const char *val);

/**
 * Decode a binary key-value pair.
 *
 * @param[in]   ptr     Pointer to data buffer containing encoded data
 * @param[in]   len     Length of data buffer containing encoded data
 * @param[in]   field   Key of value to be decoded in encoded data buffer
 * @param[out]  val     Value to be retrieved from encoded data buffer
 * @param[in]   maxlen  Maximum length of value to be retrieved
 *
 * @return 0 on success, BLADERF_ERR_* on failure
 */
int binkv_decode_field(
    char *ptr, int len, char *field, char *val, size_t maxlen);

/**
 * Add a binary key-value pair to an existing binkv data buffer.
 *
 * @param[in]    buf    Buffer to add field to
 * @param[in]    len    Length of `buf' in bytes
 * @param[in]    field  Key of value to be stored in encoded data buffer
 * @param[in]    val    Value associated with key `field'
 *
 * @return 0 on success, BLADERF_ERR_* on failure
 */
int binkv_add_field(char *buf, int len, const char *field, const char *val);

#endif
