// test_temp_alerts.c
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include "temp_sensor_alerts.h"   // put IOCTL structs/defines here

int main(void)
{
    int fd;
    struct temp_thresholds th = {
        .low_mdeg  = 15 * 1000,
        .high_mdeg = 35 * 1000,
    };

    fd = open("/dev/temp_sensor_alerts", O_RDONLY);
    if (fd < 0) { perror("open"); return 1; }

    if (ioctl(fd, TEMP_IOC_SET_THRESHOLDS, &th) < 0)
        perror("ioctl SET_THRESHOLDS");

    printf("Waiting for alerts...\n");
    while (1) {
        struct temp_alert_user a;
        ssize_t n = read(fd, &a, sizeof(a));   // blocks until alert
        if (n < 0) { perror("read"); break; }

        printf("ALERT #%d: type=%d temp=%.3f C\n",
               a.alert_id, a.type, a.temp_mdeg / 1000.0);
    }

    close(fd);
    return 0;
}

