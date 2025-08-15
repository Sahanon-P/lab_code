#include <stdio.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <linux/ioctl.h>

#define DEVICE "/dev/mycdrv"
#define MY_IOCTL_MAGIC 'k'

int main(int argc, char *argv[])
{
    int fd, len;
    char *buf;
    unsigned int cmd;

    if (argc != 2) {
        printf("Usage: %s <length>\n", argv[0]);
        return 1;
    }

    len = atoi(argv[1]);
    if (len <= 0 || len > 4096) {
        printf("Length must be between 1 and 4096\n");
        return 1;
    }

    buf = malloc(len);
    if (!buf) {
        perror("malloc");
        return 1;
    }
    memset(buf, 0, len);

    printf("User buffer before ioctl: '%s'\n", buf);

    fd = open(DEVICE, O_RDWR);
    if (fd < 0) {
        perror("open");
        free(buf);
        return 1;
    }

    // Compose the ioctl command with the desired length
    cmd = _IOWR(MY_IOCTL_MAGIC, 1, char[len]);

    if (ioctl(fd, cmd, buf) < 0) {
        perror("ioctl");
        close(fd);
        free(buf);
        return 1;
    }

    printf("User buffer after ioctl: '%s'\n", buf);

    close(fd);
    free(buf);
    return 0;
}