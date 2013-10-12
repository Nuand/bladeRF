/*
 * bladeRF USB driver
 * Copyright (C) 2013 Nuand LLC
 * Copyright (C) 2013 Robert Ghilduta
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/usb.h>
#include <linux/slab.h>
#include <linux/scatterlist.h>
#include <linux/dma-mapping.h>
#include <linux/sched.h>
#include <linux/mutex.h>
#include <linux/wait.h>
#include <linux/uaccess.h>
#include "../../../firmware_common/bladeRF.h"

struct data_buffer {
    struct urb  *urb;
    void        *addr;
    dma_addr_t   dma;
    int          valid;
};

typedef struct {
    struct usb_device    *udev;
    struct usb_interface *interface;

    int                   intnum;
    int                   disconnecting;

    int                   rx_en;
    spinlock_t            data_in_lock;
    unsigned int          data_in_consumer_idx;
    unsigned int          data_in_producer_idx;
    atomic_t              data_in_cnt;          // number of buffers with data unread by the usermode application
    atomic_t              data_in_used;         // number of buffers that may be inflight or have unread data
    atomic_t              data_in_inflight;     // number of buffers currently in the USB stack
    struct data_buffer    data_in_bufs[NUM_DATA_URB];
    struct usb_anchor     data_in_anchor;
    wait_queue_head_t     data_in_wait;

    int                   tx_en;
    spinlock_t            data_out_lock;
    unsigned int          data_out_consumer_idx;
    unsigned int          data_out_producer_idx;
    atomic_t              data_out_cnt;
    atomic_t              data_out_used;
    atomic_t              data_out_inflight;
    struct data_buffer    data_out_bufs[NUM_DATA_URB];
    struct usb_anchor     data_out_anchor;
    wait_queue_head_t     data_out_wait;

    struct semaphore      config_sem;

    struct file           *reader, *writer;

    int bytes;
    int debug;
} bladerf_device_t;

static struct usb_driver bladerf_driver;

// USB PID-VID table
static struct usb_device_id bladerf_table[] = {
    { USB_DEVICE(USB_NUAND_VENDOR_ID, USB_NUAND_BLADERF_PRODUCT_ID) },
    { } /* Terminate entry */
};
MODULE_DEVICE_TABLE(usb, bladerf_table);

static int __submit_rx_urb(bladerf_device_t *dev, unsigned int flags) {
    struct urb *urb;
    unsigned long irq_flags;
    int ret;

    ret = 0;
    while (atomic_read(&dev->data_in_inflight) < NUM_CONCURRENT && atomic_read(&dev->data_in_used) < NUM_DATA_URB) {
        spin_lock_irqsave(&dev->data_in_lock, irq_flags);
        urb = dev->data_in_bufs[dev->data_in_producer_idx].urb;

        if (!dev->data_in_bufs[dev->data_in_producer_idx].valid) {
            spin_unlock_irqrestore(&dev->data_in_lock, irq_flags);
            // break;
        }

        dev->data_in_bufs[dev->data_in_producer_idx].valid = 0; // mark this RX packet as being in use

        dev->data_in_producer_idx++;
        dev->data_in_producer_idx &= (NUM_DATA_URB - 1);
        atomic_inc(&dev->data_in_inflight);
        atomic_inc(&dev->data_in_used);

        usb_anchor_urb(urb, &dev->data_in_anchor);
        spin_unlock_irqrestore(&dev->data_in_lock, irq_flags);
        ret = usb_submit_urb(urb, GFP_ATOMIC);
    }

    return ret;
}

static void __bladeRF_write_cb(struct urb *urb);
static void __bladeRF_read_cb(struct urb *urb) {
    bladerf_device_t *dev;
    unsigned char *buf;

    buf = (unsigned char *)urb->transfer_buffer;
    dev = (bladerf_device_t *)urb->context;
    usb_unanchor_urb(urb);
    atomic_dec(&dev->data_in_inflight);
    dev->bytes += DATA_BUF_SZ;
    atomic_inc(&dev->data_in_cnt);

    if (dev->rx_en)
        __submit_rx_urb(dev, GFP_ATOMIC);
    wake_up_interruptible(&dev->data_in_wait);
}

static int bladerf_start(bladerf_device_t *dev) {
    int i;
    void *buf;
    struct urb *urb;

    dev->rx_en = 0;
    atomic_set(&dev->data_in_cnt, 0);
    atomic_set(&dev->data_in_used, 0);
    dev->data_in_consumer_idx = 0;
    dev->data_in_producer_idx = 0;

    for (i = 0; i < NUM_DATA_URB; i++) {
        buf = usb_alloc_coherent(dev->udev, DATA_BUF_SZ,
                GFP_KERNEL, &dev->data_in_bufs[i].dma);
        memset(buf, 0, DATA_BUF_SZ);
        if (!buf) {
            dev_err(&dev->interface->dev, "Could not allocate data IN buffer\n");
            return -1;
        }

        dev->data_in_bufs[i].addr = buf;

        urb = usb_alloc_urb(0, GFP_KERNEL);
        if (!buf) {
            dev_err(&dev->interface->dev, "Could not allocate data IN URB\n");
            return -1;
        }

        dev->data_in_bufs[i].urb = urb;
        dev->data_in_bufs[i].valid = 1;

        usb_fill_bulk_urb(urb, dev->udev, usb_rcvbulkpipe(dev->udev, 1),
                dev->data_in_bufs[i].addr, DATA_BUF_SZ, __bladeRF_read_cb, dev);

        urb->transfer_flags |= URB_NO_TRANSFER_DMA_MAP;
        urb->transfer_dma = dev->data_in_bufs[i].dma;
    }

    dev->tx_en = 0;
    atomic_set(&dev->data_out_cnt, 0);
    atomic_set(&dev->data_out_used, 0);

    dev->data_out_consumer_idx = 0;
    dev->data_out_producer_idx = 0;

    for (i = 0; i < NUM_DATA_URB; i++) {
        buf = usb_alloc_coherent(dev->udev, DATA_BUF_SZ,
                GFP_KERNEL, &dev->data_out_bufs[i].dma);
        memset(buf, 0, DATA_BUF_SZ);
        if (!buf) {
            dev_err(&dev->interface->dev, "Could not allocate data OUT buffer\n");
            return -1;
        }

        dev->data_out_bufs[i].addr = buf;

        urb = usb_alloc_urb(0, GFP_KERNEL);
        if (!buf) {
            dev_err(&dev->interface->dev, "Could not allocate data OUT URB\n");
            return -1;
        }

        dev->data_out_bufs[i].urb = urb;
        dev->data_out_bufs[i].valid = 0;

        usb_fill_bulk_urb(urb, dev->udev, usb_sndbulkpipe(dev->udev, 1),
                dev->data_out_bufs[i].addr, DATA_BUF_SZ, __bladeRF_write_cb, dev);

        urb->transfer_flags |= URB_NO_TRANSFER_DMA_MAP;
        urb->transfer_dma = dev->data_out_bufs[i].dma;
    }


    return 0;
}

static void bladerf_stop(bladerf_device_t *dev) {
    int i;
    for (i = 0; i < NUM_DATA_URB; i++) {
        usb_free_coherent(dev->udev, DATA_BUF_SZ, dev->data_in_bufs[i].addr, dev->data_in_bufs[i].dma);
        usb_free_urb(dev->data_in_bufs[i].urb);
        usb_free_coherent(dev->udev, DATA_BUF_SZ, dev->data_out_bufs[i].addr, dev->data_out_bufs[i].dma);
        usb_free_urb(dev->data_out_bufs[i].urb);
    }
}

int __bladerf_snd_cmd(bladerf_device_t *dev, int cmd, void *ptr, __u16 len);
static int disable_tx(bladerf_device_t *dev) {
    int ret;
    unsigned int val;
    val = 0;

    if (dev->intnum != 1)
        return -1;

    dev->tx_en = 0;

    usb_kill_anchored_urbs(&dev->data_out_anchor);

    ret = __bladerf_snd_cmd(dev, BLADE_USB_CMD_RF_TX, &val, sizeof(val));
    if (ret < 0)
        goto err_out;

    ret = 0;

err_out:
    return ret;
}

static int enable_tx(bladerf_device_t *dev) {
    int ret;
    unsigned int val;
    val = 1;

    if (dev->intnum != 1)
        return -1;

    ret = __bladerf_snd_cmd(dev, BLADE_USB_CMD_RF_TX, &val, sizeof(val));
    if (ret < 0)
        goto err_out;

    ret = 0;
    dev->tx_en = 1;

err_out:
    return ret;
}

static int disable_rx(bladerf_device_t *dev) {
    int ret;
    unsigned int val;
    val = 0;

    if (dev->intnum != 1)
        return -1;

    dev->rx_en = 0;

    usb_kill_anchored_urbs(&dev->data_in_anchor);

    ret = __bladerf_snd_cmd(dev, BLADE_USB_CMD_RF_RX, &val, sizeof(val));
    if (ret < 0)
        goto err_out;

    ret = 0;
    atomic_set(&dev->data_in_cnt, 0);
    atomic_set(&dev->data_in_used, 0);
    dev->data_in_consumer_idx = 0;
    dev->data_in_producer_idx = 0;

err_out:
    return ret;
}

static int enable_rx(bladerf_device_t *dev) {
    int ret;
    int i;
    unsigned int val;
    val = 1;

    if (dev->intnum != 1)
        return -1;

    if (dev->disconnecting)
        return -ENODEV;

    for (i = 0; i < NUM_DATA_URB; i++)
        dev->data_in_bufs[i].valid = 1;

    ret = __bladerf_snd_cmd(dev, BLADE_USB_CMD_RF_RX, &val, sizeof(val));
    if (ret < 0)
        goto err_out;

    ret = 0;
    dev->rx_en = 1;

    for (i = 0; i < NUM_CONCURRENT; i++) {
        if ((ret = __submit_rx_urb(dev, 0)) < 0) {
            dev_err(&dev->interface->dev, "Error submitting initial RX URBs (%d/%d), error=%d\n", i, NUM_CONCURRENT, ret);
            break;
        }
    }

err_out:
    return ret;
}

static ssize_t bladerf_read(struct file *file, char __user *buf, size_t count, loff_t *ppos)
{
    ssize_t ret = 0;
    bladerf_device_t *dev;
    unsigned long flags;
    int read;

    dev = (bladerf_device_t *)file->private_data;
    if (dev->intnum != 1) {
        return -1;
    }

    if (dev->reader) {
        if (file != dev->reader) {
            return -EPERM;
        }
    } else
        dev->reader = file;

    if (dev->disconnecting)
        return -ENODEV;

    if (!dev->rx_en) {
        if (enable_rx(dev)) {
            return -EINVAL;
        }
    }
    read = 0;

    while (!read) {
        int reread;
        reread = atomic_read(&dev->data_in_cnt);

        if (reread) {
            unsigned int idx;

            spin_lock_irqsave(&dev->data_in_lock, flags);
            atomic_dec(&dev->data_in_cnt);
            atomic_dec(&dev->data_in_used);
            idx = dev->data_in_consumer_idx++;
            dev->data_in_consumer_idx &= (NUM_DATA_URB - 1);

            spin_unlock_irqrestore(&dev->data_in_lock, flags);

            if (copy_to_user(buf, dev->data_in_bufs[idx].addr, DATA_BUF_SZ)) {
                ret = -EFAULT;
            } else {
                ret = 0;
            }


            dev->data_in_bufs[idx].valid = 1; // mark this RX packet as free

            // in case all of the buffers were full, rx needs to be restarted
            // samples may have also been dropped if this happens because the user-mode
            // application is not reading samples fast enough
            if (atomic_read(&dev->data_in_inflight) == 0)
                __submit_rx_urb(dev, 0);


            if (!ret)
                ret = DATA_BUF_SZ;

            break;

        } else {
            ret = wait_event_interruptible_timeout(dev->data_in_wait, atomic_read(&dev->data_in_cnt), 2 * HZ);
            if (ret < 0) {
                break;
            } else if (ret == 0) {
                ret = -ETIMEDOUT;
                break;
            } else {
                ret = 0;
            }
        }
    }

    return ret;
}

static int __submit_tx_urb(bladerf_device_t *dev) {
    struct urb *urb;
    struct data_buffer *db;
    unsigned long flags;

    int ret = 0;

    while (atomic_read(&dev->data_out_inflight) < NUM_CONCURRENT && atomic_read(&dev->data_out_cnt)) {
        spin_lock_irqsave(&dev->data_out_lock, flags);
        db = &dev->data_out_bufs[dev->data_out_consumer_idx];
        urb = db->urb;

        if (!db->valid) {
            spin_unlock_irqrestore(&dev->data_out_lock, flags);
            // break;
        }

        // clear this packet's valid flag so it is not submitted until the next time it
        // is used and copy_from_user() has copied data into the buffer
        db->valid = 0;
        dev->data_out_consumer_idx++;
        dev->data_out_consumer_idx &= (NUM_DATA_URB - 1);

        atomic_dec(&dev->data_out_cnt);

        usb_anchor_urb(urb, &dev->data_out_anchor);
        spin_unlock_irqrestore(&dev->data_out_lock, flags);
        ret = usb_submit_urb(urb, GFP_ATOMIC);
        if (!ret)
            atomic_inc(&dev->data_out_inflight);
        else
            break;
    }

    return ret;
}

static void __bladeRF_write_cb(struct urb *urb)
{
    bladerf_device_t *dev;

    dev = (bladerf_device_t *)urb->context;

    usb_unanchor_urb(urb);

    atomic_dec(&dev->data_out_inflight);
    atomic_dec(&dev->data_out_used);
    if (dev->tx_en)
        __submit_tx_urb(dev);
    dev->bytes += DATA_BUF_SZ;
    wake_up_interruptible(&dev->data_out_wait);
}

static ssize_t bladerf_write(struct file *file, const char *user_buf, size_t count, loff_t *ppos)
{
    bladerf_device_t *dev;
    unsigned long flags;
    char *buf = NULL;
    struct data_buffer *db = NULL;
    unsigned int idx;
    int reread;
    int status = 0;

    /* TODO truncate count to be within range of ssize_t here? */

    dev = (bladerf_device_t *)file->private_data;

    if (dev->intnum == 0) {
        int llen;
        buf = (char *)kmalloc(count, GFP_KERNEL);
        if (buf) {
            if (copy_from_user(buf, user_buf, count)) {
                status = -EFAULT;
            } else {
                status = usb_bulk_msg(dev->udev, usb_sndbulkpipe(dev->udev, 2), buf, count, &llen, BLADE_USB_TIMEOUT_MS);
            }
            kfree(buf);
        } else {
            dev_err(&dev->interface->dev, "Failed to allocate write buffer\n");
            status = -ENOMEM;
        }

        if (status < 0)
            return status;
        else
            return llen;
    }

    if (dev->writer) {
        if (file != dev->writer) {
            return -EPERM;
        }
    } else
        dev->writer = file;

    reread = atomic_read(&dev->data_out_used);
    if (reread >= NUM_DATA_URB) {
        status = wait_event_interruptible_timeout(dev->data_out_wait, atomic_read(&dev->data_out_used) < NUM_DATA_URB, 2 * HZ);

        if (status < 0) {
            return status;
        } else if (status == 0) {
            return -ETIMEDOUT;
        }
    }

    spin_lock_irqsave(&dev->data_out_lock, flags);

    idx = dev->data_out_producer_idx++;
    dev->data_out_producer_idx &= (NUM_DATA_URB - 1);
    db = &dev->data_out_bufs[idx];
    atomic_inc(&dev->data_out_cnt);
    atomic_inc(&dev->data_out_used);

    spin_unlock_irqrestore(&dev->data_out_lock, flags);

    if (copy_from_user(db->addr, user_buf, count)) {
        return -EFAULT;
    }

    db->valid = 1; // mark this TX packet as having valid data

    __submit_tx_urb(dev);
    if (!dev->tx_en)
        enable_tx(dev);

    return count;
}


int __bladerf_rcv_cmd(bladerf_device_t *dev, int cmd, void *ptr, __u16 len) {
    int tries = 3;
    int retval;

    do {
        retval = usb_control_msg(dev->udev, usb_rcvctrlpipe(dev->udev, 0),
                cmd, BLADE_USB_TYPE_IN, 0, 0,
                ptr, len, BLADE_USB_TIMEOUT_MS);
        if (retval < 0) {
            dev_err(&dev->interface->dev, "Error in %s calling usb_control_msg()"
                    " with error %d, %d tries left\n", __func__, retval, tries);
        }
    } while ((retval < 0) && --tries);

    return retval;
}

int __bladerf_rcv_one_word(bladerf_device_t *dev, int cmd, void __user *arg) {
    unsigned int buf;
    int retval = -EINVAL;

    if (!arg) {
        retval = -EFAULT;
        goto err_out;
    }

    retval = __bladerf_rcv_cmd(dev, cmd, &buf, sizeof(buf));

    if (retval >= 0) {
        buf = le32_to_cpu(buf);
        if (copy_to_user(arg, &buf, sizeof(buf))) {
            retval = -EFAULT;
        } else {
            retval = 0;
        }
    }

    if (retval >= 0) {
        retval = 0;
    }

err_out:
    return retval;
}

int __bladerf_snd_cmd(bladerf_device_t *dev, int cmd, void *ptr, __u16 len) {
    int tries = 3;
    int retval;

    do {
        printk("usb_control_msg(ptr=%p) len=%d\n", ptr, len);
        retval = usb_control_msg(dev->udev, usb_sndctrlpipe(dev->udev, 0),
                cmd, BLADE_USB_TYPE_OUT, 0, 0,
                ptr, len, BLADE_USB_TIMEOUT_MS);
        printk("done usb_control_msg() = %d\n", retval);
        if (retval < 0) {
            dev_err(&dev->interface->dev, "Error in %s calling usb_control_msg()"
                    " with error %d, %d tries left\n", __func__, retval, tries);
        }
    } while ((retval < 0) && --tries);

    return retval;
}

int __bladerf_snd_one_word(bladerf_device_t *dev, int cmd, void __user *arg) {
    unsigned int buf;
    int retval = -EINVAL;

    if (!arg) {
        retval = -EFAULT;
        goto err_out;
    }

    if ((retval = copy_from_user(&buf, arg, sizeof(buf))))
        goto err_out;
    buf = cpu_to_le32(buf);

    retval = __bladerf_snd_cmd(dev, cmd, &buf, sizeof(buf));

err_out:
    return retval;
}

long bladerf_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    bladerf_device_t *dev;
    void __user *data;
    struct bladerf_fx3_version ver;
    int ret;
    int retval = -EINVAL;
    int sz, nread, nwrite;
    struct uart_cmd spi_reg;
    int sectors_to_wipe, sector_idx;
    int pages_to_write, page_idx;
    int pages_to_read;
    int check_idx;
    int count, tries;
    int targetdev;

    /* FIXME this large buffer should be kmalloc'd and kept with the dev, no? */
    unsigned char buf[1024];
    struct bladeRF_firmware brf_fw;
    struct bladeRF_sector brf_sector;
    unsigned char *fw_buf;

    dev = file->private_data;
    data = (void __user *)arg;


    switch (cmd) {
        case BLADE_QUERY_VERSION:
            retval = __bladerf_rcv_cmd(dev, BLADE_USB_CMD_QUERY_VERSION, &ver, sizeof(ver));
            if (retval >= 0) {
                ver.major = le16_to_cpu(ver.major);
                ver.minor = le16_to_cpu(ver.minor);
                if (copy_to_user(data, &ver, sizeof(struct bladerf_fx3_version))) {
                    retval = -EFAULT;
                } else {
                    retval = 0;
                }
            }
            break;

        case BLADE_QUERY_FPGA_STATUS:
            retval = __bladerf_rcv_one_word(dev, BLADE_USB_CMD_QUERY_FPGA_STATUS, data);
            break;

        case BLADE_BEGIN_PROG:
            if (dev->intnum != 0) {
                ret = usb_set_interface(dev->udev, 0,0);
                dev->intnum = 0;
            }

            retval = __bladerf_rcv_one_word(dev, BLADE_USB_CMD_BEGIN_PROG, data);
            break;

        case BLADE_END_PROG:
            // TODO: send another 2 DCLK cycles to ensure compliance with C4's boot procedure
            retval = __bladerf_rcv_one_word(dev, BLADE_USB_CMD_QUERY_FPGA_STATUS, data);

            if (!retval) {
                ret = usb_set_interface(dev->udev, 0,1);
                dev->intnum = 1;
            }
            break;

        case BLADE_CAL:
            if (dev->intnum != 2) {
                retval = usb_set_interface(dev->udev, 0,2);

                if (retval)
                    break;

                dev->intnum = 2;
            }
            if (copy_from_user(&brf_fw, data, sizeof(struct bladeRF_firmware))) {
                return -EFAULT;
            }

            fw_buf = kzalloc(256, GFP_KERNEL);
            if (!fw_buf)
                return -EINVAL;

            memset(fw_buf, 0xff, 256);

            if (copy_from_user(fw_buf, brf_fw.ptr, brf_fw.len)) {
                retval = -EFAULT;
                break;
            }

            retval = usb_control_msg(dev->udev, usb_rcvctrlpipe(dev->udev, 0),
                    BLADE_USB_CMD_FLASH_ERASE, BLADE_USB_TYPE_IN, 0x0000, 3,
                    &ret, 4, BLADE_USB_TIMEOUT_MS * 100);

            if (!retval) {
                dev_err(&dev->interface->dev, "Could not erase NAND cal sector 3.\n");
                break;
            }

            retval = usb_control_msg(dev->udev, usb_rcvctrlpipe(dev->udev, 0),
                    BLADE_USB_CMD_FLASH_WRITE, BLADE_USB_TYPE_OUT, 0x0000, 768,
                    fw_buf, 256, BLADE_USB_TIMEOUT_MS);

            if (!retval) {
                dev_err(&dev->interface->dev, "Could not write NAND cal sector 768.\n");
                break;
            }

            memset(buf, 0, 256);

            retval = usb_control_msg(dev->udev, usb_rcvctrlpipe(dev->udev, 0),
                    BLADE_USB_CMD_FLASH_READ, BLADE_USB_TYPE_IN, 0x0000, 768,
                    buf, 256, BLADE_USB_TIMEOUT_MS);

            if (!retval) {
                dev_err(&dev->interface->dev, "Could not read NAND cal sector 768.\n");
                break;
            }

            retval = memcmp(fw_buf, buf, 256);
            break;

        case BLADE_FLASH_ERASE:
                retval = usb_control_msg(dev->udev, usb_rcvctrlpipe(dev->udev, 0),
                        BLADE_USB_CMD_FLASH_ERASE, BLADE_USB_TYPE_IN, 0x0000, arg,
                        &ret, 4, BLADE_USB_TIMEOUT_MS * 100);

                if (!retval) {
                    dev_err(&dev->interface->dev, "Could not read NAND cal sector 768.\n");
                    break;
                }

                retval = !ret;
        break;

        case BLADE_OTP:
            if (dev->intnum != 2) {
                retval = usb_set_interface(dev->udev, 0,2);

                if (retval)
                    break;

                dev->intnum = 2;
            }

            if (copy_from_user(&brf_fw, data, sizeof(struct bladeRF_firmware))) {
                return -EFAULT;
            }

            fw_buf = kzalloc(256, GFP_KERNEL);
            if (!fw_buf)
                return -EINVAL;
            memset(fw_buf, 0xff, 256);

            if (copy_from_user(fw_buf, brf_fw.ptr, brf_fw.len)) {
                retval = -EFAULT;
                kfree(fw_buf);
                break;
            }

            memcpy(buf, fw_buf, brf_fw.len);

            retval = usb_control_msg(dev->udev, usb_rcvctrlpipe(dev->udev, 0),
                    BLADE_USB_CMD_WRITE_OTP, BLADE_USB_TYPE_OUT, 0x0000, 0,
                    fw_buf, 256, BLADE_USB_TIMEOUT_MS);

            if (!retval) {
                dev_err(&dev->interface->dev, "Could not write OTP.\n");
                break;
            }

            memset(buf, 0, 256);

            retval = usb_control_msg(dev->udev, usb_rcvctrlpipe(dev->udev, 0),
                    BLADE_USB_CMD_READ_OTP, BLADE_USB_TYPE_IN, 0x0000, 0,
                    buf, 256, BLADE_USB_TIMEOUT_MS);

            if (!retval) {
                dev_err(&dev->interface->dev, "Could not read OTP.\n");
                break;
            }

            retval = memcmp(fw_buf, buf, 256);
            break;

        case BLADE_OTP_READ:
        case BLADE_FLASH_READ:
        case BLADE_FLASH_WRITE:
            if (dev->intnum != 2) {
                retval = usb_set_interface(dev->udev, 0,2);

                if (retval)
                    break;

                dev->intnum = 2;
            }

            if (copy_from_user(&brf_sector, data, sizeof(struct bladeRF_sector))) {
                return -EFAULT;
            }

            if (cmd == BLADE_OTP_READ) {
                if (brf_sector.idx != 0 || brf_sector.len != 0x100)
                    dev_err(&dev->interface->dev, "Invalid OTP settings, expecting idx=0, len=256\n");
            }


            sz = 0;
            if (dev->udev->speed == USB_SPEED_HIGH) {
                sz = 64;
            } else if (dev->udev->speed == USB_SPEED_SUPER) {
                sz = 256;
            }

            count = brf_sector.len + (sz - (brf_sector.len % sz));
            fw_buf = kzalloc(count, GFP_KERNEL);

            if (!fw_buf)
                return -EFAULT;

            memset(fw_buf, 0xff, count);

            if (cmd == BLADE_FLASH_READ || cmd == BLADE_OTP_READ) {
                pages_to_read = (brf_sector.len + 255) / 0x100;
                nread = 0;
                for (page_idx = 0; page_idx < pages_to_read; page_idx++) {
                    do {
                        retval = usb_control_msg(dev->udev, usb_rcvctrlpipe(dev->udev, 0),
                                (cmd == BLADE_FLASH_READ) ? BLADE_USB_CMD_FLASH_READ : BLADE_USB_CMD_READ_OTP,
                                BLADE_USB_TYPE_IN, 0x0000, brf_sector.idx + page_idx,
                                &fw_buf[nread], sz, BLADE_USB_TIMEOUT_MS);
                        printk("%d read %d bytes %x %x %x %x\n", retval, sz, fw_buf[nread], fw_buf[nread+1], fw_buf[nread+2], fw_buf[nread+3]);
                        nread += sz;
                        if (retval != sz) break;
                    } while (nread != 256);
                    if (retval != sz) break;
                }

                if (!retval) {
                    dev_err(&dev->interface->dev, "Could not read NAND cal page idx %d.\n", page_idx);
                    break;
                }
                if (copy_to_user((void __user *)brf_sector.ptr, fw_buf, brf_sector.len)) {
                    retval = -EFAULT;
                    break;
                }
            } else if (cmd == BLADE_FLASH_WRITE) {
                if (copy_from_user(fw_buf, brf_sector.ptr, brf_sector.len)) {
                    retval = -EFAULT;
                    break;
                }

                pages_to_write = (brf_sector.len + 255) / 0x100;
                nwrite = 0;
                for (page_idx = 0; page_idx < pages_to_write; page_idx++) {
                    do {
                        retval = usb_control_msg(dev->udev, usb_sndctrlpipe(dev->udev, 0),
                                BLADE_USB_CMD_FLASH_WRITE, BLADE_USB_TYPE_OUT, 0x0000, brf_sector.idx + page_idx,
                                &fw_buf[page_idx * 256 + nwrite], sz, BLADE_USB_TIMEOUT_MS);
                        nwrite += sz;
                        if (retval != sz) break;
                    } while (nwrite != 256);
                    if (retval != sz) break;
                }

                if (!retval) {
                    dev_err(&dev->interface->dev, "Could not write NAND cal page idx %d.\n", page_idx);
                    break;
                }
            }

            break;

        case BLADE_UPGRADE_FW:

            if (dev->intnum != 2) {
                retval = usb_set_interface(dev->udev, 0,2);

                if (retval)
                    break;

                dev->intnum = 2;
            }

            if (copy_from_user(&brf_fw, data, sizeof(struct bladeRF_firmware))) {
                return -EFAULT;
            }

            brf_fw.len = ((brf_fw.len + 255) / 256) * 256;

            fw_buf = kzalloc(brf_fw.len, GFP_KERNEL);
            if (!fw_buf)
                goto leave_fw;

            if (copy_from_user(fw_buf, brf_fw.ptr, brf_fw.len)) {
                retval = -EFAULT;
                goto leave_fw;
            }

            retval = -ENODEV;

            sectors_to_wipe = (brf_fw.len + 0xffff) / 0x10000;
            printk("Going to wipe %d sectors\n", sectors_to_wipe);
            for (sector_idx = 0; sector_idx < sectors_to_wipe; sector_idx++) {
                printk("Erasing sector %d... ", sector_idx);
                retval = usb_control_msg(dev->udev, usb_rcvctrlpipe(dev->udev, 0),
                        BLADE_USB_CMD_FLASH_ERASE, BLADE_USB_TYPE_IN, 0x0000, sector_idx,
                        &ret, 4, BLADE_USB_TIMEOUT_MS * 100);
                printk("- erased\n");

                if (retval != 4) {
                    goto leave_fw;
                }
                ret = le32_to_cpu(ret);
                if (ret != 1) {
                    printk("Unable to erase previous sector, quitting\n");
                    goto leave_fw;
                }
            }

            sz = 0;
            if (dev->udev->speed == USB_SPEED_HIGH) {
                sz = 64;
            } else if (dev->udev->speed == USB_SPEED_SUPER) {
                sz = 256;
            }

            pages_to_write = (brf_fw.len + 255) / 0x100;
            for (page_idx = pages_to_write - 1; page_idx >= 0; page_idx--) {
                nwrite = 0;
                do {
                    retval = usb_control_msg(dev->udev, usb_sndctrlpipe(dev->udev, 0),
                            BLADE_USB_CMD_FLASH_WRITE, BLADE_USB_TYPE_OUT, 0x0000, page_idx,
                            &fw_buf[page_idx * 256 + nwrite], sz, BLADE_USB_TIMEOUT_MS);
                    nwrite += sz;
                } while (nwrite != 256);
            }

            pages_to_read = (brf_fw.len + 255) / 0x100;

            for (page_idx = 0; page_idx < pages_to_read; page_idx++) {
                nread = 0;
                do {
                    retval = usb_control_msg(dev->udev, usb_rcvctrlpipe(dev->udev, 0),
                            BLADE_USB_CMD_FLASH_READ, BLADE_USB_TYPE_IN, 0x0000, page_idx,
                            &buf[nread], sz, BLADE_USB_TIMEOUT_MS);
                    nread += sz;
                } while (nread != 256);

                for (check_idx = 0; check_idx < 256; check_idx++) {
                    if (buf[check_idx] != fw_buf[page_idx * 256 + check_idx]) {
                        printk("ERROR: bladeRF firmware verification detected a mismatch at byte offset 0x%.8x\n", page_idx * 256 + check_idx);
                        printk("ERROR: expected byte 0x%.2X, got 0x%.2X\n", fw_buf[page_idx * 256 + check_idx], buf[check_idx]);
                        retval = -EINVAL;
                        goto leave_fw;
                    }
                }
            }
            retval = 0;
            printk("SUCCESSFULLY VERIFIED\n");

leave_fw:
            kfree(fw_buf);
            break;

        case BLADE_DEVICE_RESET:
            ret = 1;
            retval = __bladerf_snd_cmd(dev, BLADE_USB_CMD_RESET, &ret, sizeof(ret));
            break;


        case BLADE_CHECK_PROG:
            retval = 0;
            printk("ok %d\n", dev->intnum);
            if (dev->intnum == 0) {
                retval = __bladerf_rcv_cmd(dev, BLADE_USB_CMD_QUERY_FPGA_STATUS, &ret, sizeof(ret));
                printk("retval =%d     ret=%d\n", retval, ret);
                if (retval >= 0 && ret) {
                    retval = 0;
                    ret = usb_set_interface(dev->udev, 0,1);
                    dev->intnum = 1;

                    if (copy_to_user((void __user *)arg, &ret, sizeof(ret))){
                        retval = -EFAULT;
                    } else {
                        retval = 0;
                    }
                }
            }
            break;

        case BLADE_RF_RX:
            if (dev->intnum != 1) {
                dev_err(&dev->interface->dev, "Cannot enable RX from config mode\n");
                retval = -1;
                break;
            }

            printk("RF_RX!\n");
            retval = __bladerf_snd_one_word(dev, BLADE_USB_CMD_RF_RX, data);
            break;

        case BLADE_RF_TX:
            if (dev->intnum != 1) {
                dev_err(&dev->interface->dev, "Cannot enable TX from config mode\n");
                retval = -1;
                break;
            }

            printk("RF_TX!\n");
            retval = __bladerf_snd_one_word(dev, BLADE_USB_CMD_RF_TX, data);
            break;

        case BLADE_LMS_WRITE:
        case BLADE_LMS_READ:
        case BLADE_SI5338_WRITE:
        case BLADE_SI5338_READ:
        case BLADE_GPIO_WRITE:
        case BLADE_GPIO_READ:
        case BLADE_VCTCXO_WRITE:

            if (copy_from_user(&spi_reg, (void __user *)arg, sizeof(struct uart_cmd))) {
                retval = -EFAULT;
                break;
            }

            nread = count = 16;
            memset(buf, 0, 20);
            buf[0] = 'N';

            targetdev = UART_PKT_DEV_SI5338;
            if (cmd == BLADE_GPIO_WRITE || cmd == BLADE_GPIO_READ)
                targetdev = UART_PKT_DEV_GPIO;
            if (cmd == BLADE_LMS_WRITE || cmd == BLADE_LMS_READ)
                targetdev = UART_PKT_DEV_LMS;
            if (cmd == BLADE_VCTCXO_WRITE)
                targetdev = UART_PKT_DEV_VCTCXO;

            if (cmd == BLADE_LMS_WRITE || cmd == BLADE_GPIO_WRITE || cmd == BLADE_SI5338_WRITE || cmd == BLADE_VCTCXO_WRITE) {
                buf[1] = UART_PKT_MODE_DIR_WRITE | targetdev | 0x01;
                buf[2] = spi_reg.addr;
                buf[3] = spi_reg.data;
            } else if (cmd == BLADE_LMS_READ || cmd == BLADE_GPIO_READ || cmd == BLADE_SI5338_READ) {
                buf[1] = UART_PKT_MODE_DIR_READ | targetdev | 0x01;
                buf[2] = spi_reg.addr;
                buf[3] = 0xff;
            }

            retval = usb_bulk_msg(dev->udev, usb_sndbulkpipe(dev->udev, 2), buf, count, &nread, BLADE_USB_TIMEOUT_MS);
            if (!retval) {
                memset(buf, 0, 20);

                tries = 3;
                do {
                    retval = usb_bulk_msg(dev->udev, usb_rcvbulkpipe(dev->udev, 0x82), buf, count, &nread, BLADE_USB_TIMEOUT_MS);
                } while(retval == -ETIMEDOUT && tries--);

                if (!retval) {
                    spi_reg.addr = buf[2];
                    spi_reg.data = buf[3];
                }

                if (copy_to_user((void __user *)arg, &spi_reg, sizeof(struct uart_cmd))) {
                    retval = -EFAULT;
                } else {
                    retval = 0;
                }
            }
            break;

        case BLADE_GET_SPEED:
            ret = dev->udev->speed == USB_SPEED_SUPER;
            if (copy_to_user((void __user *)arg, &ret, sizeof(ret))) {
                retval = -EFAULT;
            } else {
                retval = 0;
            }
            break;

        case BLADE_GET_ADDR:
            ret = dev->udev->devnum;
            if (copy_to_user((void __user *)arg, &ret, sizeof(ret))) {
                retval = -EFAULT;
            } else {
                retval = 0;
            }
            break;

        case BLADE_GET_BUS:
            ret = dev->udev->bus->busnum;
            if (copy_to_user((void __user *)arg, &ret, sizeof(ret))) {
                retval = -EFAULT;
            } else {
                retval = 0;
            }
            break;

    }

    return retval;
}

static int bladerf_open(struct inode *inode, struct file *file)
{
    bladerf_device_t *dev;
    struct usb_interface *interface;
    int subminor;

    subminor = iminor(inode);

    interface = usb_find_interface(&bladerf_driver, subminor);
    if (interface == NULL) {
        pr_err("%s - error, cannot find device for minor %d\n", __func__, subminor);
        return -ENODEV;
    }

    dev = usb_get_intfdata(interface);
    if (dev == NULL) {
        return -ENODEV;
    }

    file->private_data = dev;

    return 0;
}

static int bladerf_release(struct inode *inode, struct file *file)
{
    bladerf_device_t *dev;

    dev = (bladerf_device_t *)file->private_data;

    if (dev->writer && dev->writer == file) {
        if (dev->tx_en) {
            disable_tx(dev);
        }
        dev->writer = NULL;
    }

    if (dev->reader && dev->reader == file) {
        if (dev->rx_en) {
            disable_rx(dev);
        }
        dev->reader = NULL;
    }

    return 0;
}

static struct file_operations bladerf_fops = {
    .owner    =  THIS_MODULE,
    .read     =  bladerf_read,
    .write    =  bladerf_write,
    .unlocked_ioctl = bladerf_ioctl,
    .open     =  bladerf_open,
    .release  =  bladerf_release,
};

static struct usb_class_driver bladerf_class = {
    .name       = "bladerf%d",
    .fops       = &bladerf_fops,
    .minor_base = USB_NUAND_BLADERF_MINOR_BASE,
};

static int bladerf_probe(struct usb_interface *interface,
        const struct usb_device_id *id)
{
    bladerf_device_t *dev;
    int retval;
    if (interface->cur_altsetting->desc.bInterfaceNumber != 0)
        return 0;

    dev = kzalloc(sizeof(bladerf_device_t), GFP_KERNEL);
    if (dev == NULL) {
        dev_err(&interface->dev, "Out of memory\n");
        goto error_oom;
    }


    spin_lock_init(&dev->data_in_lock);
    spin_lock_init(&dev->data_out_lock);
    dev->udev = usb_get_dev(interface_to_usbdev(interface));
    dev->interface = interface;
    dev->intnum = 0;
    dev->bytes = 0;
    dev->debug = 0;
    dev->disconnecting = 0;

    atomic_set(&dev->data_in_inflight, 0);
    atomic_set(&dev->data_out_inflight, 0);

    init_usb_anchor(&dev->data_in_anchor);
    init_waitqueue_head(&dev->data_in_wait);

    init_usb_anchor(&dev->data_out_anchor);
    init_waitqueue_head(&dev->data_out_wait);

    bladerf_start(dev);

    usb_set_intfdata(interface, dev);

    retval = usb_register_dev(interface, &bladerf_class);
    if (retval) {
        dev_err(&interface->dev, "Unable to get a minor device number for bladeRF device\n");
        usb_set_intfdata(interface, NULL);
        return retval;
    }

    dev_info(&interface->dev, "Nuand bladeRF device is now attached\n");
    return 0;

error_oom:
    return -ENOMEM;
}

static void bladerf_disconnect(struct usb_interface *interface)
{
    bladerf_device_t *dev;
    if (interface->cur_altsetting->desc.bInterfaceNumber != 0)
        return;

    dev = usb_get_intfdata(interface);

    dev->disconnecting = 1;
    dev->tx_en = 0;
    dev->rx_en = 0;

    usb_kill_anchored_urbs(&dev->data_out_anchor);
    usb_kill_anchored_urbs(&dev->data_in_anchor);

    bladerf_stop(dev);

    usb_deregister_dev(interface, &bladerf_class);

    usb_set_intfdata(interface, NULL);

    usb_put_dev(dev->udev);

    dev_info(&interface->dev, "Nuand bladeRF device has been disconnected\n");

    kfree(dev);
}

static struct usb_driver bladerf_driver = {
    .name = "nuand_bladerf",
    .probe = bladerf_probe,
    .disconnect = bladerf_disconnect,
    .id_table = bladerf_table,
};

module_usb_driver(bladerf_driver);

MODULE_AUTHOR("Robert Ghilduta <robert.ghilduta@gmail.com>");
MODULE_DESCRIPTION("bladeRF USB driver");
MODULE_VERSION("v0.2");
MODULE_LICENSE("GPL");
