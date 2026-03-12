#include "ata.h"
#include "serial.h"

/* ATA PIO ports (secondary bus 0x170-0x177 for storage disk) */
#define ATA_PORT_DATA       0x170u
#define ATA_PORT_ERROR      0x171u
#define ATA_PORT_SECT_COUNT 0x172u
#define ATA_PORT_LBA_LO     0x173u
#define ATA_PORT_LBA_MID    0x174u
#define ATA_PORT_LBA_HI     0x175u
#define ATA_PORT_DRIVE      0x176u
#define ATA_PORT_STATUS     0x177u
#define ATA_PORT_CMD        0x177u
#define ATA_PORT_CTRL       0x376u

#define ATA_CMD_READ_PIO    0x20u
#define ATA_CMD_WRITE_PIO   0x30u
#define ATA_CMD_IDENTIFY    0xECu
#define ATA_CMD_FLUSH       0xE7u

#define ATA_STATUS_BSY      0x80u
#define ATA_STATUS_DRDY     0x40u
#define ATA_STATUS_DRQ      0x08u
#define ATA_STATUS_ERR      0x01u

#define ATA_DRIVE_MASTER    0xE0u
#define ATA_DRIVE_SLAVE     0xF0u

#define ATA_TIMEOUT         1000000u
#define ATA_MAX_SECTORS     256u

static int g_ata_available = 0;

static inline void ata_outb(uint16_t port, uint8_t val) {
    asm volatile("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint8_t ata_inb(uint16_t port) {
    uint8_t ret;
    asm volatile("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline void ata_outw(uint16_t port, uint16_t val) {
    asm volatile("outw %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint16_t ata_inw(uint16_t port) {
    uint16_t ret;
    asm volatile("inw %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static int ata_wait_ready(void) {
    uint32_t timeout = ATA_TIMEOUT;

    while (timeout > 0) {
        uint8_t status = ata_inb(ATA_PORT_STATUS);

        if (status == 0xFF) {
            return 0;
        }

        if (!(status & ATA_STATUS_BSY)) {
            if (status & ATA_STATUS_ERR) {
                return 0;
            }

            return 1;
        }

        timeout--;
    }

    return 0;
}

static int ata_wait_drq(void) {
    uint32_t timeout = ATA_TIMEOUT;

    while (timeout > 0) {
        uint8_t status = ata_inb(ATA_PORT_STATUS);

        if (status == 0xFF) {
            return 0;
        }

        if (status & ATA_STATUS_ERR) {
            return 0;
        }

        if (!(status & ATA_STATUS_BSY) && (status & ATA_STATUS_DRQ)) {
            return 1;
        }

        timeout--;
    }

    return 0;
}

static void ata_400ns_delay(void) {
    /* Read status port 4 times for ~400ns delay */
    ata_inb(ATA_PORT_CTRL);
    ata_inb(ATA_PORT_CTRL);
    ata_inb(ATA_PORT_CTRL);
    ata_inb(ATA_PORT_CTRL);
}

static void ata_select_drive(uint32_t lba) {
    ata_outb(ATA_PORT_DRIVE, (uint8_t)(ATA_DRIVE_MASTER | ((lba >> 24) & 0x0F)));
    ata_400ns_delay();
}

int ata_init(void) {
    uint8_t status;
    uint16_t identify_buf[256];
    uint32_t i;

    g_ata_available = 0;

    /* Software reset */
    ata_outb(ATA_PORT_CTRL, 0x04);
    ata_400ns_delay();
    ata_outb(ATA_PORT_CTRL, 0x00);
    ata_400ns_delay();

    /* Select master drive */
    ata_outb(ATA_PORT_DRIVE, ATA_DRIVE_MASTER);
    ata_400ns_delay();

    /* Clear sector count and LBA registers */
    ata_outb(ATA_PORT_SECT_COUNT, 0);
    ata_outb(ATA_PORT_LBA_LO, 0);
    ata_outb(ATA_PORT_LBA_MID, 0);
    ata_outb(ATA_PORT_LBA_HI, 0);

    /* Send IDENTIFY */
    ata_outb(ATA_PORT_CMD, ATA_CMD_IDENTIFY);
    ata_400ns_delay();

    status = ata_inb(ATA_PORT_STATUS);
    if (status == 0 || status == 0xFF) {
        serial_print("[ata] secondary: no device\r\n");
        return 0;
    }

    if (!ata_wait_ready()) {
        serial_print("[ata] secondary: timeout\r\n");
        return 0;
    }

    /* Check non-ATA */
    if (ata_inb(ATA_PORT_LBA_MID) != 0 || ata_inb(ATA_PORT_LBA_HI) != 0) {
        serial_print("[ata] secondary: not ATA\r\n");
        return 0;
    }

    if (!ata_wait_drq()) {
        serial_print("[ata] secondary: DRQ timeout\r\n");
        return 0;
    }

    for (i = 0; i < 256; i++) {
        identify_buf[i] = ata_inw(ATA_PORT_DATA);
    }

    g_ata_available = 1;
    serial_print("[ata] secondary: OK\r\n");
    return 1;
}

int ata_is_available(void) {
    return g_ata_available;
}

int ata_read_sectors(uint32_t lba, uint32_t count, void* buffer, uint32_t buffer_size) {
    uint8_t* buf = (uint8_t*)buffer;
    uint32_t s;
    uint32_t i;

    if (!g_ata_available || !buffer || count == 0) {
        return 0;
    }

    if (count > ATA_MAX_SECTORS) {
        return 0;
    }

    if (buffer_size < count * ATA_SECTOR_SIZE) {
        return 0;
    }

    for (s = 0; s < count; s++) {
        uint32_t cur_lba = lba + s;

        ata_select_drive(cur_lba);

        if (!ata_wait_ready()) {
            return 0;
        }

        ata_outb(ATA_PORT_SECT_COUNT, 1);
        ata_outb(ATA_PORT_LBA_LO, (uint8_t)(cur_lba & 0xFF));
        ata_outb(ATA_PORT_LBA_MID, (uint8_t)((cur_lba >> 8) & 0xFF));
        ata_outb(ATA_PORT_LBA_HI, (uint8_t)((cur_lba >> 16) & 0xFF));

        ata_outb(ATA_PORT_CMD, ATA_CMD_READ_PIO);
        ata_400ns_delay();

        if (!ata_wait_drq()) {
            return 0;
        }

        for (i = 0; i < 256; i++) {
            uint16_t word = ata_inw(ATA_PORT_DATA);
            uint32_t offset = s * ATA_SECTOR_SIZE + i * 2;
            buf[offset] = (uint8_t)(word & 0xFF);
            buf[offset + 1] = (uint8_t)((word >> 8) & 0xFF);
        }
    }

    return 1;
}

int ata_write_sectors(uint32_t lba, uint32_t count, const void* buffer, uint32_t buffer_size) {
    const uint8_t* buf = (const uint8_t*)buffer;
    uint32_t s;
    uint32_t i;

    if (!g_ata_available || !buffer || count == 0) {
        return 0;
    }

    if (count > ATA_MAX_SECTORS) {
        return 0;
    }

    if (buffer_size < count * ATA_SECTOR_SIZE) {
        return 0;
    }

    for (s = 0; s < count; s++) {
        uint32_t cur_lba = lba + s;

        ata_select_drive(cur_lba);

        if (!ata_wait_ready()) {
            return 0;
        }

        ata_outb(ATA_PORT_SECT_COUNT, 1);
        ata_outb(ATA_PORT_LBA_LO, (uint8_t)(cur_lba & 0xFF));
        ata_outb(ATA_PORT_LBA_MID, (uint8_t)((cur_lba >> 8) & 0xFF));
        ata_outb(ATA_PORT_LBA_HI, (uint8_t)((cur_lba >> 16) & 0xFF));

        ata_outb(ATA_PORT_CMD, ATA_CMD_WRITE_PIO);
        ata_400ns_delay();

        if (!ata_wait_drq()) {
            return 0;
        }

        for (i = 0; i < 256; i++) {
            uint32_t offset = s * ATA_SECTOR_SIZE + i * 2;
            uint16_t word = (uint16_t)buf[offset] | ((uint16_t)buf[offset + 1] << 8);
            ata_outw(ATA_PORT_DATA, word);
        }

        /* Flush after each sector */
        ata_outb(ATA_PORT_CMD, ATA_CMD_FLUSH);
        if (!ata_wait_ready()) {
            return 0;
        }
    }

    return 1;
}
