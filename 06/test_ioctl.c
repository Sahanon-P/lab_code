// Place this in test_ioctl.c
#include <stdio.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <string.h>
#include <unistd.h>

#define DEVICE "/dev/mycdrv"
#define BUF_SIZE 32
#define MY_IOCTL_MAGIC  'k'
#define MY_IOCTL_RW     _IOWR(MY_IOCTL_MAGIC, 1, char[BUF_SIZE])

int main() {
    int fd;
    char buf[BUF_SIZE] = "Hello from user!";
    printf("User buffer before ioctl: '%s'\n", buf);

    fd = open(DEVICE, O_RDWR);
    if (fd < 0) {
        perror("open");
        return 1;
    }

    if (ioctl(fd, MY_IOCTL_RW, buf) < 0) {
        perror("ioctl");
        close(fd);
        return 1;
    }

    printf("User buffer after ioctl: '%s'\n", buf);

    close(fd);
    return 0;
}