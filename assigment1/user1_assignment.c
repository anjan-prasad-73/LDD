#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <string.h>

#define IOCTL_ON __IOW(IOCTL_MAGIC,1,int);
#define IOCTL_OFF __IOW(IOCTL_MAGIC,2,int);
void menu()
{
    printf("\n===== LIGHT CONTROLLER  MENU =====\n");
    printf("1. ON/OFF \n");
    printf("2. WRITE \n");
	printf("3. READ  \n");
    printf("Select: ");
}

int main()
{   int  fd = open("/dev/CONTROLLER", O_RDWR);
    if (fd < 0) {
        perror("open");
        return 1;
    }
	int cmd;
	int choice;
	int u_buff[2];
	int u_buff_read[3];
	while(1)
	{
	menu();
	scanf("%d",&choice);
	if(choice==1)
	{
		printf("ENTER 1:ON 0:OFF :-");
		scnaf("%d",&cmd);
		if(cmd==1)
		{
            if (ioctl(fd, IOCTL_ON,0) < 0)
	         perror("WRITE ON ");
        }
		else if(cmd==0)
		{
		if (ioctl(fd, IOCTL_OFF, 0) < 0)
	         perror("WRITE OFF ");
		}
	}
	else if(choice==2)
	{
	if(cmd)
	{
	printf("ENTER BRIGHTNESS(10-100):");
	scanf("%d",&u_buff[0]);
	printf("ENTER TEMPERATURE(2000-6500):");
	scanf("%d",&u_buff[1]);
	
    if (write(fd, &u_buff, sizeof(u_buff)) != sizeof(u_buff)) {
        perror("write");
        close(fd);
        return 1;
    }
	}
	}
	else if(choice==3)
	{
		int len=read(fd, u_buff_read, size);
		if (len < 0)
                perror("read");
            else {
                printf("BRIGHTNESS: %s\n", u_buff_read[0]);
				printf("TEMPERATURE: %s\n", u_buff_read[1]);
				printf("BRIGHTNESS: %s\n", u_buff_read[2]);
            }
	}
	else{
		printf("invalid choice");
	}
