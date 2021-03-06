// Copyright 2015-2019 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <stdlib.h>
#include "hal/spi_flash_hal.h"
#include "string.h"
#include "hal/hal_defs.h"

#define ADDRESS_MASK_24BIT 0xFFFFFF

static inline spi_dev_t *get_spi_dev(spi_flash_host_driver_t *chip_drv)
{
    return ((spi_flash_memspi_data_t *)chip_drv->driver_data)->spi;
}

void spi_flash_hal_poll_cmd_done(spi_flash_host_driver_t *driver)
{
    while (!spi_flash_ll_cmd_is_done(get_spi_dev(driver))) {
        //nop
    }
}

esp_err_t spi_flash_hal_device_config(spi_flash_host_driver_t *driver)
{
    spi_flash_memspi_data_t *drv_data = (spi_flash_memspi_data_t *)driver->driver_data;
    spi_dev_t *dev = get_spi_dev(driver);
    spi_flash_ll_reset(dev);
    spi_flash_ll_set_cs_pin(dev, drv_data->cs_num);
    spi_flash_ll_set_clock(dev, &drv_data->clock_conf);
    return ESP_OK;
}

esp_err_t spi_flash_hal_configure_host_read_mode(spi_flash_host_driver_t *driver, esp_flash_read_mode_t read_mode,
        uint32_t addr_bitlen, uint32_t dummy_cyclelen_base,
        uint32_t read_command)
{
    // Add dummy cycles to compensate for latency of GPIO matrix and external delay, if necessary...
    int dummy_cyclelen = dummy_cyclelen_base + ((spi_flash_memspi_data_t *)driver->driver_data)->extra_dummy;

    spi_dev_t *dev = get_spi_dev(driver);
    spi_flash_ll_set_addr_bitlen(dev, addr_bitlen);
    spi_flash_ll_set_command8(dev, read_command);
    spi_flash_ll_read_phase(dev);
    spi_flash_ll_set_dummy(dev, dummy_cyclelen);
    spi_flash_ll_set_read_mode(dev, read_mode);
    return ESP_OK;
}

esp_err_t spi_flash_hal_common_command(spi_flash_host_driver_t *chip_drv, spi_flash_trans_t *trans)
{
    chip_drv->configure_host_read_mode(chip_drv, SPI_FLASH_FASTRD, 0, 0, 0);
    spi_dev_t *dev = get_spi_dev(chip_drv);
    spi_flash_ll_set_command8(dev, trans->command);
    spi_flash_ll_set_addr_bitlen(dev, 0);
    spi_flash_ll_set_miso_bitlen(dev, trans->miso_len);
    spi_flash_ll_set_mosi_bitlen(dev, trans->mosi_len);

    spi_flash_ll_write_word(dev, trans->mosi_data);

    spi_flash_ll_user_start(dev);
    chip_drv->poll_cmd_done(chip_drv);
    spi_flash_ll_get_buffer_data(dev, trans->miso_data, 8);
    return ESP_OK;
}

void spi_flash_hal_erase_chip(spi_flash_host_driver_t *chip_drv)
{
    spi_dev_t *dev = get_spi_dev(chip_drv);
    spi_flash_ll_erase_chip(dev);
    chip_drv->poll_cmd_done(chip_drv);
}

void spi_flash_hal_erase_sector(spi_flash_host_driver_t *chip_drv, uint32_t start_address)
{
    spi_dev_t *dev = get_spi_dev(chip_drv);
    spi_flash_ll_set_addr_bitlen(dev, 24);
    spi_flash_ll_set_address(dev, start_address & ADDRESS_MASK_24BIT);
    spi_flash_ll_erase_sector(dev);
    chip_drv->poll_cmd_done(chip_drv);
}

void spi_flash_hal_erase_block(spi_flash_host_driver_t *chip_drv, uint32_t start_address)
{
    spi_dev_t *dev = get_spi_dev(chip_drv);
    spi_flash_ll_set_addr_bitlen(dev, 24);
    spi_flash_ll_set_address(dev, start_address & ADDRESS_MASK_24BIT);
    spi_flash_ll_erase_block(dev);
    chip_drv->poll_cmd_done(chip_drv);
}

void spi_flash_hal_program_page(spi_flash_host_driver_t *chip_drv, const void *buffer, uint32_t address, uint32_t length)
{
    spi_dev_t *dev = get_spi_dev(chip_drv);
    spi_flash_ll_set_addr_bitlen(dev, 24);
    spi_flash_ll_set_address(dev, (address & ADDRESS_MASK_24BIT) | (length << 24));
    spi_flash_ll_program_page(dev, buffer, length);
    chip_drv->poll_cmd_done(chip_drv);
}

esp_err_t spi_flash_hal_read(spi_flash_host_driver_t *chip_drv, void *buffer, uint32_t address, uint32_t read_len)
{
    spi_dev_t *dev = get_spi_dev(chip_drv);
    //the command is already set by ``spi_flash_hal_configure_host_read_mode`` before.
    spi_flash_ll_set_address(dev, address << 8);
    spi_flash_ll_set_miso_bitlen(dev, read_len * 8);
    spi_flash_ll_user_start(dev);
    chip_drv->poll_cmd_done(chip_drv);
    spi_flash_ll_get_buffer_data(dev, buffer, read_len);
    return ESP_OK;
}


bool spi_flash_hal_host_idle(spi_flash_host_driver_t *chip_drv)
{
    spi_dev_t *dev = get_spi_dev(chip_drv);
    bool idle = spi_flash_ll_host_idle(dev);

    // Not clear if this is necessary, or only necessary if
    // chip->spi == SPI1. But probably doesn't hurt...
    if (dev == &SPI1) {
        idle &= spi_flash_ll_host_idle(&SPI0);
    }

    return idle;
}

esp_err_t spi_flash_hal_set_write_protect(spi_flash_host_driver_t *chip_drv, bool wp)
{
    spi_dev_t *dev = get_spi_dev(chip_drv);
    spi_flash_ll_set_write_protect(dev, wp);
    chip_drv->poll_cmd_done(chip_drv);
    return ESP_OK;
}
