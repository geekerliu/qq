#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <errno.h>
#include "common.h"

#define MAX_STDIN_BUF 200
#define MAX_MSG_LEN  4096

static void command_help(uint8_t* arg);
static void command_list(uint8_t* arg);
static void command_send(uint8_t* arg);
static int socket_fd;
static usr_command my_command[] = {
	{
		.command_seq = 0,
		.command_name = "help",
		.command_desc = "list all command",
		.command_fun = command_help,
	},
	{
		.command_seq = 1,
		.command_name = "list",
		.command_desc = "list who on line",
		.command_fun = command_list,
	},
	{
		.command_seq = 2,
		.command_name = "send",
		.command_desc = "Usage:send/dst_fd/content",
		.command_fun = command_send,
	},
};

/*--------------------------------------------------------------*/

static void command_help(uint8_t* arg)
{
	int i;

	printf("------------------------------------------\n");
	printf("%-10s%-10s%-100s\n", "seq", "name", "desc");
	for(i = 0; i < sizeof(my_command)/sizeof(my_command[0]); i++)
	{
		printf("%-10d%-10s%-100s\n", 
			my_command[i].command_seq, my_command[i].command_name, my_command[i].command_desc);
	}
	printf("------------------------------------------\n");
}

static void command_list(uint8_t* arg)
{
	uint32_t command_len;
	command_msg com_list;
	
	/* 构建消息结构体 */
	memset(&com_list, 0, sizeof(command_msg));
	command_len = sizeof(command_msg);
	com_list.type = LIST;

	/* 发送消息 */
	write(socket_fd, &com_list, command_len);
}

static void command_send(uint8_t* arg)
{
	command_msg* msg_ptr;
	uint32_t dst_fd;
	uint8_t* argv[3];
	uint16_t value_len, msg_len;
	
	if(arg == NULL)
		return;

	argv[0] = strtok(arg, "/");/* 分隔出第一个字段 */
	if(argv[0])
	{
		if(strcmp("send", argv[0]) != 0)
		{
			printf("Usage:send fd content\n");
			return;
		}
	}
	
	argv[1] = strtok(NULL, "/");/* 分隔出client_fd字段 */
	if(argv[1] == NULL)
	{
		printf("Usage:send fd content\n");
		return;
	}
	if(atoi(argv[1]) == 0)
	{
		printf("client fd is not a num or too small.\n");
		return;
	}

	argv[2] = strtok(NULL, "/");/* 分隔出内容字段 */
	if(argv[2] == NULL)
	{
		printf("Usage:send fd content\n");
		return;
	}
	
	value_len = strlen(argv[2]);
	msg_len = sizeof(command_msg) + value_len;
	msg_ptr = (command_msg*)malloc(msg_len);
	memset(msg_ptr, 0, msg_len);
	msg_ptr->type = USER_MSG;
	msg_ptr->dst_fd = atoi(argv[1]);
	msg_ptr->len = value_len;
	memcpy((uint8_t*)(msg_ptr + 1), argv[2], value_len);

	if(-1 == write(socket_fd, msg_ptr, msg_len))
	{
		perror("write");
	}

	free(msg_ptr);
}

int deal_user_commond(uint8_t* buf, uint16_t len)
{
	int i;
	uint8_t command_len;

	/* 查找命令，找到后执行对应的函数，没有找到返回-1 */
	for(i = 0; i < sizeof(my_command)/sizeof(my_command[0]); i++)
	{
		command_len = strlen(my_command[i].command_name);
		if(strncmp(buf, my_command[i].command_name, command_len) == 0)
		{
			my_command[i].command_fun(buf);
			return 0;
		}
	}
	return -1;
}

/*--------------------------------------------------------------*/

static void client_list_ack(command_msg* msg_ptr)
{
	uint16_t client_num;
	uint8_t* value_ptr;

	client_num = msg_ptr->len / sizeof(uint32_t);
	printf("\nNow Have --%d-- client online.\n", client_num);
	value_ptr = (uint8_t *)(msg_ptr + 1);
	while(client_num--)
	{
		printf("client fd = %d\n", *(uint32_t*)value_ptr);
		value_ptr += sizeof(uint32_t);
	}
}

static void deal_server_msg(command_msg* msg_ptr)
{
	switch(msg_ptr->type)
	{
	case LIST_ACK:
		client_list_ack(msg_ptr);
		break;
	case MSG_ACK:
		printf("\nserver:have no this client-->%d\n", *(uint32_t*)(msg_ptr + 1));
		break;
	case USER_MSG:
		printf("\nclient %d:%s\n", msg_ptr->src_fd, (uint8_t*)(msg_ptr + 1));
		break;
	default:
		printf("\nunknown command.\n");
		break;
	}
}

/*--------------------------------------------------------------*/

int main(int argc, char const *argv[])
{
	struct sockaddr_in server_addr = {0};
	fd_set all_read, old;
	uint8_t stdin_buf[MAX_STDIN_BUF];
	uint8_t socket_buf[MAX_SOCKET_BUF];
	int len;
	command_msg* user_command_msg, *command_msg_ptr;
	int command_len;
	
	if((socket_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
	{
		perror("socket");
		exit(-1);
	}
	
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(PORT);
	server_addr.sin_addr.s_addr = inet_addr("192.168.0.107");
	if(-1 == connect(socket_fd, (struct sockaddr*)(&server_addr), sizeof(server_addr)))
	{
		perror("connect");
		exit(-1);
	}
	
	FD_ZERO(&all_read);
	FD_SET(STDIN_FILENO, &all_read);
	FD_SET(socket_fd, &all_read);
	old  = all_read;
	while(1)
	{
		printf("please input:");
		fflush(stdout);
		all_read = old;
		if(select(socket_fd + 1, &all_read, NULL, NULL, NULL) > 0)
		{			
			if(FD_ISSET(STDIN_FILENO, &all_read))
			{
				/* 读取用户输入 */
				bzero(stdin_buf, MAX_STDIN_BUF);
				gets(stdin_buf);

				/* 处理用户命令 */
				if(-1 == deal_user_commond(stdin_buf, strlen(stdin_buf)))
				{
					printf("Unknown Command.\n");
					continue;
				}
			}
			
			if(FD_ISSET(socket_fd, &all_read))
			{
				bzero(socket_buf, sizeof(socket_buf));
				len = read(socket_fd, socket_buf, MAX_SOCKET_BUF);
				if(len == 0)
				{
					printf("Server Down.\n");
					exit(0);
				}
				
				if(len < sizeof(command_msg))
					continue;
					
				command_msg_ptr = (command_msg*)socket_buf;
				deal_server_msg(command_msg_ptr);
			}
		}
	}

	return 0;
}
