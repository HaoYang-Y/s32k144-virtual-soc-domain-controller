/**
 * spi_loopback_test - 简单的 SPI 自环测试
 * 发送递增数据，接收后比对，验证 SPI 硬件正常
 *
 * 用法: gcc spidev_test.c -o spidev_test && ./spidev_test /dev/spidev0.1
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/spi/spidev.h>

#define BUF_SIZE 64

int main(int argc, char *argv[])
{
    const char *device = (argc > 1) ? argv[1] : "/dev/spidev0.1";
    uint8_t tx[BUF_SIZE];
    uint8_t rx[BUF_SIZE];
    int fd, ret;
    uint8_t mode = SPI_MODE_0;
    uint8_t bits  = 8;
    uint32_t speed = 1000000;

    fd = open(device, O_RDWR);
    if (fd < 0) {
        perror("open");
        return 1;
    }

    /* 配置 SPI 参数 */
    ret = ioctl(fd, SPI_IOC_WR_MODE, &mode);
    if (ret < 0) { perror("SPI_IOC_WR_MODE"); goto out; }

    ret = ioctl(fd, SPI_IOC_WR_BITS_PER_WORD, &bits);
    if (ret < 0) { perror("SPI_IOC_WR_BITS_PER_WORD"); goto out; }

    ret = ioctl(fd, SPI_IOC_WR_MAX_SPEED_HZ, &speed);
    if (ret < 0) { perror("SPI_IOC_WR_MAX_SPEED_HZ"); goto out; }

    printf("Device  : %s\n", device);
    printf("Mode    : %u\n", mode);
    printf("Bits    : %u\n", bits);
    printf("Speed   : %u Hz\n", speed);

    /* 发送递增数据 */
    for (int i = 0; i < BUF_SIZE; i++) tx[i] = (uint8_t)i;

    struct spi_ioc_transfer tr = {
        .tx_buf        = (unsigned long)tx,
        .rx_buf        = (unsigned long)rx,
        .len           = BUF_SIZE,
        .speed_hz      = speed,
        .bits_per_word = bits,
    };

    ret = ioctl(fd, SPI_IOC_MESSAGE(1), &tr);
    if (ret < 0) {
        perror("SPI_IOC_MESSAGE");
        goto out;
    }

    printf("Sent (%d bytes): ", BUF_SIZE);
    for (int i = 0; i < 16; i++) printf("%02X ", tx[i]);
    printf("...\n");

    printf("Recv (%d bytes): ", BUF_SIZE);
    for (int i = 0; i < 16; i++) printf("%02X ", rx[i]);
    printf("...\n");

    /* 比对 */
    if (memcmp(tx, rx, BUF_SIZE) == 0) {
        printf("\n  PASS: TX == RX, SPI loopback OK!\n");
    } else {
        printf("\n  FAIL: TX != RX (check MO-MI short)\n");
    }

out:
    close(fd);
    return ret;
}
