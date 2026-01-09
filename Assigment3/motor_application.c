#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <string.h>
/* Motor modes */
enum motor_mode {
    MOTOR_MODE_NORMAL = 0,
    MOTOR_MODE_REVERE,
    MOTOR_MODE_BRAKE,
};

/* Returned by read() */
struct motor_state {
    int speed;      /* arbitrary speed units */
    int mode;       /* one of enum motor_mode */
};

/* IOCTL definitions */
#define MOTOR_IOCTL_MAGIC   'M'
#define MOTOR_IOCTL_SET_MODE  _IOW(MOTOR_IOCTL_MAGIC, 1, int)
#define MOTOR_IOCTL_GET_MODE  _IOR(MOTOR_IOCTL_MAGIC, 2, int)

int main()
{
    int fd;
    int speed, mode;
    struct motor_state state;

    fd = open("/dev/motor0", O_RDWR);
    if (fd < 0) {
        perror("open");
        return 1;
    }

    while (1) {
        printf("\n===== Motor Control Menu =====\n");
        printf("1. Set Mode\n");
        printf("2. Set Speed\n");
        printf("3. Get State\n");
        printf("4. Get Mode\n");
        printf("5. Exit\n");
        printf("Choose option: ");
        
        int opt;
        scanf("%d", &opt);

        switch (opt) {

        case 1:
            printf("Enter Mode (0=NORMAL, 1=REVERSE, 2=BRAKE): ");
            scanf("%d", &mode);

            if (ioctl(fd, MOTOR_IOCTL_SET_MODE, &mode) < 0)
                perror("SET_MODE");
            else
                printf("Mode set to %d\n", mode);
            break;

        case 2:
            printf("Enter Speed: ");
            scanf("%d", &speed);

            if (write(fd, &speed, sizeof(speed)) != sizeof(speed))
                perror("write");
            else
                printf("Speed set to %d\n", speed);
            break;

        case 3:
            lseek(fd, 0, SEEK_SET);
            if (read(fd, &state, sizeof(state)) != sizeof(state))
                perror("read");
            else
                printf("State: speed=%d, mode=%d\n",
                       state.speed, state.mode);
            break;

        case 4:
            if (ioctl(fd, MOTOR_IOCTL_GET_MODE, &mode) < 0)
                perror("GET_MODE");
            else
                printf("Current mode: %d\n", mode);
            break;

        case 5:
            close(fd);
            printf("Exiting...\n");
            return 0;

        default:
            printf("Invalid option!\n");
        }
    }

    close(fd);
    return 0;
}
