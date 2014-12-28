#include <stdio.h>
#include "common.h"
#include <termios.h>  
#include <unistd.h>  
#include <errno.h>  
#define ECHOFLAGS (ECHO | ECHOE | ECHOK | ECHONL)

//#define DUBUG
/**************************************
*函数set_disp_mode用于控制是否开启输入回显功能  
*如果option为0，则关闭回显，为1则打开回显
**************************************/
int set_disp_mode(int fd,int option)  
{  
   int err;  
   struct termios term;  
   if(tcgetattr(fd,&term)==-1){  
     perror("Cannot get the attribution of the terminal");  
     return 1;  
   }  
   if(option)  
        term.c_lflag|=ECHOFLAGS;  
   else  
        term.c_lflag &=~ECHOFLAGS;  
   err=tcsetattr(fd,TCSAFLUSH,&term);  
   if(err==-1 && err==EINTR){  
        perror("Cannot set the attribution of the terminal");  
        return 1;  
   }  
   return 0;  
} 

int main (int argc, char *argv[])
{
	int fd = open("/home/farsight/samba/server/record.txt",O_RDWR | O_APPEND | O_CREAT, 0666);
	if(-1 == fd)
	{
		perror("open");
		return -1;
	}
	Record buf;
	bzero(&buf,sizeof(buf));
	#ifdef DUBUG
	/*打印出当前所有注册的用户*/	
	lseek(fd, 0, SEEK_SET);
	while(read(fd, &buf, sizeof(buf)))
	{
		printf("%s,%s\n",buf.name,buf.passwd);
	}
	#endif
	
	
	/*注册*/
	while(1)
	{
		char temp[50];
		printf("Input your name:");fflush(stdout);
		gets(buf.name);
		set_disp_mode(STDIN_FILENO,0);
		printf("Input your password:");fflush(stdout);
		gets(buf.passwd);
		putchar('\n');
		printf("Input your password again:");fflush(stdout);
		gets(temp);
		putchar('\n');
		set_disp_mode(STDIN_FILENO,1); 
		if( 0 != strcmp(temp,buf.passwd))
		{
			printf("Two input password is not the same!\n");
			continue;
		}
		else
		{
			lseek(fd, 0 , SEEK_END);
			write(fd,&buf,sizeof(buf));
			
			printf("Continue to register? y/n\n");
			gets(temp);
			if('y' == *temp)
			{
				continue;	
			}
			else
			{
				break;
			}
		}
	}
	#ifdef DUBUG
	/*打印出当前所有注册的用户*/	
	lseek(fd, 0, SEEK_SET);
	while(read(fd, &buf, sizeof(buf)))
	{
		printf("%s,%s\n",buf.name,buf.passwd);
	}
	#endif
	return 0;
}
