#include<stdio.h>
#include<stdlib.h>
#include<sys/types.h>
#include<sys/stat.h>
#include<fcntl.h>
#include<unistd.h>
#include<pthread.h>
#include<semaphore.h>

int rc = 0, fd;
char read_buf;

void* driver_write(void *ws)
{
	int rc = 0;
	int i = 0;
	
	char *write_string;	
	printf("\n Writing to Driver \n");
	write_string = (char*)ws;
	/* using the system call 'write' to write in the driver */
	while (write_string[i] != '\0'){
	rc = write(fd,&write_string[i],1);	
	printf("\nCharacter written: %c\n",write_string[i]);
	if(rc == -1)
	{
		perror("***** Write to driver failed !! *****\n");
		close(fd);
		exit(-1);
	}
	i++;
	
	}
} 

int main(int argc, char **argv)
{
	int i, ret_val;
	char write_string1[3]="ab",write_string2[3]="ef",write_string3[3]="ij";
	char choice;
	pthread_t wr1,wr2,wr3;
		
	/* Open our driver device */
	fd = open("/dev/hello1_sync", O_RDWR);
	if(fd == -1)
	{
		perror("***** Driver open failed !! *****\n");
		close(fd);
		rc = fd;
		exit(-1);
	}	
	/* Create 3 different threads to write to the driver*/
	pthread_create(&wr1, NULL, driver_write,(void*)write_string1);
	pthread_create(&wr2, NULL, driver_write,(void*)write_string2);
	pthread_create(&wr3, NULL, driver_write,(void*)write_string3);
		
	pthread_join(wr1,NULL);
	pthread_join(wr2,NULL);
	pthread_join(wr3,NULL);
	
	printf("\nTo read a character press (Y) Else (N):");
	while(1){
		
		fflush(NULL);
		/* accept user choice */
		choice = getchar();
		
		if(toupper(choice) == 'Y')
		{	
			ret_val = read(fd,&read_buf,1);
			if(ret_val == -1)
			{
				printf("\n**** ERROR **** Read call Failed !!\n");
				close(fd);
				return -1;
			}

		}
		else if(toupper(choice) == 'N')
		{
			break; /* exit if user does not wish to read more characters */
		}
	}
	
	close(fd);
	return 0;
}

