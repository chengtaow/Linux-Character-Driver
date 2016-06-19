#include <linux/ioctl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <time.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

#define DEVICE "/dev/mycdrv"

#define CDRV_IOC_MAGIC 'Z'
#define ASP_CHGACCDIR _IOW(CDRV_IOC_MAGIC,0x12,int)

int main(int argc, const char *argv[]){
	if (argc<2){
    		fprintf(stderr,"Device number not specified\n");
		return 1;
	}
	int dev_no = atoi(argv[1]);
	char dev_path[20];
	int fd;
	char ch,write_buf[100],read_buf[10];
	int offset,origin;
	int dir;
	sprintf(dev_path,"%s%d",DEVICE,dev_no);
	fd = open(dev_path,O_RDWR);
	if(fd == -1) {
		printf("File %s either does not exist or has been locked by another "
				"process\n", DEVICE);
		exit(-1);
	}

	printf(" r = read from device after seeking to desired offset\n"
			" w = write to device \n");
	printf(" c = reverse direction of data access");
	printf("\n\n enter command :");

	scanf("%c",&ch);
	switch(ch){    
	case 'w':
		printf("Enter data to write:\n");
		scanf(" %[^\n]",write_buf);
		int n = write(fd,write_buf, sizeof(write_buf));
		if(n < 0){
        		perror("FAIL TO WRITE\n");
        		return -1;
        	}
		break;

	case 'c':
		printf(" 0 = regular\n 1 = reverse\n");
        	printf(" \n enter direction :");
        	scanf("%d",&dir);
		int rc = ioctl(fd,ASP_CHGACCDIR,&dir);
		if (rc == -1){
			perror("\n***error in ioctl***\n");
			return -1;
		}
		break;

	case 'r':
		printf("Origin: \n 0 = beginning\n 1 = current \n 2 = end\n\n");
		printf(" enter origin :");
		scanf("%d",&origin);
		printf(" \n enter offset :");
		scanf("%d",&offset);
		llseek(fd,offset,origin);
		if (read(fd, read_buf, sizeof(read_buf)) > 0) {
			printf("\ndevice: %s\n", read_buf);
		} else {
			fprintf(stderr, "Reading failed\n");
		}
		break;

	default:
		printf("Command not recognized\n");
		break;
	}	

	close(fd);
	return 0;
}
