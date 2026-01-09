#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <sys/ioctl.h>
#include<errno.h>
#include "vblock_ioctl.h"

#define DEV_PATH "/dev/vblock0"

void menu()
{
    printf("\n===== VBLOCK CONTROL MENU =====\n");
    printf("1. Write data\n");
    printf("2. Read data\n");
    printf("3. Lock region\n");
    printf("4. Unlock region\n");
    printf("5. Read full region\n");
    printf("6. Erase region\n");
    printf("7. Get device info\n");
    printf("8. Exit\n");
    printf("9. Read MIRROR region\n");
    printf("10. Backup to file\n");
    printf("Select: ");
}

int main()
{
    int fd = open(DEV_PATH, O_RDWR);
    if (fd < 0) {
        perror("open");
        return 1;
    }

    int choice;
    char buffer[1024];

    while (1) {
        menu();
        scanf("%d", &choice);

        if (choice == 1) {
            char writebuf[1024];
            printf("\nFormat: key:offset:data\n");
            printf("Example: 111:0:HELLO\n");
            printf("Enter write string: ");
            scanf("%s", writebuf);

ssize_t len = write(fd, writebuf, strlen(writebuf));

if (len < 0) {
    if (errno == EACCES) {
        printf("ERROR: Permission denied (Region is locked, key missing or invalid)\n");
    } else {
        perror("write");
    }
} else {
    printf("Write OK (%ld bytes)\n", len);
}

        } else if (choice == 2) {
            int offset, size;
            printf("Enter offset: ");
            scanf("%d", &offset);
            printf("Enter size: ");
            scanf("%d", &size);

            lseek(fd, offset, SEEK_SET);
            int read_len = read(fd, buffer, size);
            if (read_len < 0)
                perror("read");
            else {
                buffer[read_len] = 0;
                printf("Read Data: %s\n", buffer);
            }

        } else if (choice == 3) {
            int region;
            printf("Enter region index (0-7): ");
            scanf("%d", &region);

            if (ioctl(fd, VBLOCK_LOCK_REGION, &region) < 0)
                perror("LOCK ioctl");
            else
                printf("Region %d locked.\n", region);

        } else if (choice == 4) {
            int region;
            printf("Enter region index (0-7): ");
            scanf("%d", &region);

            if (ioctl(fd, VBLOCK_UNLOCK_REGION, &region) < 0)
                perror("UNLOCK ioctl");
            else
                printf("Region %d unlocked.\n", region);

        } else if (choice == 5) {
            struct vblock_region r;
            printf("Enter region index (0-7): ");
            scanf("%d", &r.region_index);

            if (ioctl(fd, VBLOCK_READ_REGION, &r) < 0)
                perror("READ_REGION ioctl");
            else {
                printf("Region data:\n");
                write(STDOUT_FILENO, r.data, 512);
                printf("\n");
            }

        } else if (choice == 6) {
            int region;
            printf("Enter region index (0-7): ");
            scanf("%d", &region);

            if (ioctl(fd, VBLOCK_ERASE_REGION, &region) < 0)
                perror("ERASE_REGION ioctl");
            else
                printf("Region %d erased.\n", region);

        } else if (choice == 7) {
            struct vblock_info info;

            if (ioctl(fd, VBLOCK_GET_INFO, &info) < 0)
                perror("GET_INFO ioctl");
            else {
                printf("=== DEVICE INFO ===\n");
                printf("Size          : %u\n", info.size);
                printf("Region size   : %u\n", info.region_size);
                printf("Num regions   : %u\n", info.num_regions);
                printf("Lock bitmap   : 0x%02x\n", info.lock_bitmap);
            }

        } else if (choice == 8) {
            printf("Exiting...\n");
            close(fd);
            return 0;
        }

        /* ---------------------- NEW OPTION: READ MIRROR ---------------------- */
        else if (choice == 9) {
            struct vblock_region r;
            printf("Enter region index (0-7): ");
            scanf("%d", &r.region_index);

            if (ioctl(fd, VBLOCK_READ_MIRROR, &r) < 0)
                perror("READ_MIRROR ioctl");
            else {
                printf("\n--- MIRROR DATA (Region %d) ---\n", r.region_index);
                write(STDOUT_FILENO, r.data, 512);
                printf("\n");
            }
        }

        /* ---------------------- NEW OPTION: BACKUP ---------------------- */
        else if (choice == 10) {
            char path[256];
            printf("Enter filename for backup (ex: /tmp/vblock.bin): ");
            scanf("%s", path);

            if (ioctl(fd, VBLOCK_BACKUP, path) < 0)
                perror("BACKUP ioctl");
            else
                printf("Backup saved to %s\n", path);
        }

        else {
            printf("Invalid choice.\n");
        }
    }
}

