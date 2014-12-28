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

#define MAX_LISTEN_NUM 5

static client_info *client_list = NULL;/* 存储客户端节点的链表 */
static uint32_t client_num = 0;/* 当前客户端的个数 */

static void destory_client_node(uint32_t client_fd)
{
	client_info* client_info_ptr, *pre_client_info_ptr;

	for(pre_client_info_ptr = client_info_ptr = client_list; 
			client_info_ptr != NULL;
			pre_client_info_ptr = client_info_ptr, client_info_ptr = client_info_ptr->next)
	{
		if(client_info_ptr->client_fd == client_fd)
		{
			if(client_info_ptr == client_list)
			{
				client_list = client_info_ptr->next;
			}
			else
			{
				pre_client_info_ptr->next = client_info_ptr->next;
			}
			free(client_info_ptr);
			client_num--;
			return;
		}
	}
}

void list_ack(uint32_t client_fd)
{
	command_msg *list_ack;
	client_info* loop_ptr;
	uint8_t* value_ptr;
	uint32_t msg_len, value_len;

	/* 构建应答消息，将所有的client_fd依次放在value中 */
	//printf("---------------1------\n");
	value_len = (client_num - 1) * sizeof(uint32_t);/* 减掉自己的fd */
	msg_len = sizeof(command_msg) + value_len;
	list_ack = (command_msg *)malloc(msg_len);
	if(list_ack == NULL)
	{	
		perror("malloc");
		exit(-1);
	}
	memset(list_ack, 0, msg_len);
	list_ack->type = LIST_ACK;
	list_ack->len = value_len;
	value_ptr = (uint8_t*)(list_ack + 1);
	//printf("---------------2------\n");
	for(loop_ptr = client_list; loop_ptr != NULL; loop_ptr = loop_ptr->next)
	{
		if(client_fd == loop_ptr->client_fd)/* 不发送自己的fd */
			continue;
		memcpy(value_ptr, &loop_ptr->client_fd, sizeof(uint32_t));
		value_ptr += sizeof(uint32_t);
	}
	//printf("---------------3------\n");
	/* 发送应答消息 */
	if(-1 == write(client_fd, list_ack, msg_len))
	{
		perror("write");
	}
	//printf("---------------4------\n");
	/* 释放分配的空间 */
	free(list_ack);
	//printf("---------------5------\n");
}

void deal_usr_msg(command_msg* msg_ptr, uint32_t client_fd)
{
	client_info* client_info_ptr;
	uint16_t value_len, msg_len;
	command_msg * msg_ack;
	uint8_t* value_ptr;
	
	msg_ptr->src_fd = client_fd;
	for(client_info_ptr = client_list; client_info_ptr != NULL; client_info_ptr = client_info_ptr->next)
	{
		if(msg_ptr->dst_fd == client_info_ptr->client_fd)
		{
			write(msg_ptr->dst_fd, msg_ptr, get_msg_len(msg_ptr->len));
			return;
		}
	}
	
	/* 如果没有找到该用户，返回一个MSG_ACK消息 */
	value_len = strlen("hello");
	msg_len = sizeof(command_msg) + value_len;
	msg_ack = (command_msg *)malloc(msg_len);
	if(msg_ack == NULL)
	{	
		perror("malloc");
		exit(-1);
	}
	memset(msg_ack, 0, msg_len);
	msg_ack->type = MSG_ACK;
	msg_ack->len = value_len;
	value_ptr = (uint8_t*)(msg_ack + 1);
	//printf("value_len = %d, msg_len = %d\n", value_len, msg_len);
	memcpy(value_ptr, &msg_ptr->dst_fd, sizeof(msg_ptr->dst_fd));
	//printf("value = %s\n", value_ptr);
	
	/* 发送应答消息 */
	if(-1 == write(client_fd, msg_ack, msg_len))
	{
		perror("write");
	}

	/* 释放分配的空间 */
	free(msg_ack);
}

void deal_user_command(command_msg* msg_ptr, uint32_t client_fd)
{
	switch(msg_ptr->type)
	{
	case LIST:
		list_ack(client_fd);
		break;
	case USER_MSG:
		deal_usr_msg(msg_ptr, client_fd);
		break;
	default:
		printf("unknown command.\n");
		break;
	}
}

/*
*客户端处理线程，读取客户端命令->处理命令
*/
static void* client_deal_thread(void * arg)
{
	uint32_t client_fd = *(int*)arg;
	int len;
	uint8_t buf[MAX_SOCKET_BUF];
	command_msg* command_msg_ptr;

	while(1)
	{
		bzero(buf, sizeof(buf));
		len = read(client_fd, buf, MAX_SOCKET_BUF);
		if(len == 0)/* 客户端断开连接 */
		{
			printf("client %d logout.\n", client_fd);
			destory_client_node(client_fd);
			break;
		}

		if(len < sizeof(command_msg))
			continue;
			
		/* 处理用户命令 */
		printf("heheheh\n");
		command_msg_ptr = (command_msg*)buf;
		deal_user_command(command_msg_ptr, client_fd);
#if 0
		printf("---------------------------------\n"
				"message content  client fd = %d\n"
				"len = %d\n"
				"value = %s\n", 
				command_msg_ptr->client_fd,
				command_msg_ptr->len, 
				command_msg_ptr->value);
#endif
	}
	return (void*)0;
}

int main(int argc, char *argv[])
{
	struct sockaddr_in listen_addr;
	struct sockaddr_in client_addr;
	int addr_len;
	int listen_fd;
	int client_fd;
	const int sock_opt_on = 1;
	pthread_t client_pthread_fd;
	client_info* client_node_info;
	
	listen_fd = socket(AF_INET, SOCK_STREAM, 0);
	if(-1 == listen_fd)
	{
		perror("socket");
		exit(-1);
	}
	
	/* 
	* Set socket option to allow server to restart, in case
	* there are existing connections using the listening port
	*/
	setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR,(char*)&sock_opt_on,sizeof(sock_opt_on));

	addr_len = sizeof(struct sockaddr_in);
	memset((void*)&listen_addr, 0, addr_len);
	memset((void*)&client_addr, 0, addr_len);
	listen_addr.sin_family = AF_INET;
	listen_addr.sin_port = htons(PORT);
	listen_addr.sin_addr.s_addr = inet_addr("0.0.0.0");/* INADDR_ANY */

	if(-1 == bind(listen_fd, (struct sockaddr*)&listen_addr, sizeof(listen_addr)))
	{
		perror("bind");
		exit(-1);
	}

	if(-1 == listen(listen_fd, MAX_LISTEN_NUM))
	{
		perror("listen");
		exit(-1);
	}
	
	while(1)
	{
		client_fd = accept(listen_fd, (struct sockaddr*)&client_addr, &addr_len);
		if(-1 == client_fd)		
		{
			perror("accept");
			continue;
		}
		printf("Login from: %s,client_fd = %d\n", (char*)inet_ntoa(client_addr.sin_addr),client_fd);

		/* 构建客户端节点，插入客户端链表 */
		client_node_info = (client_info*)malloc(sizeof(client_info));
		if(client_node_info == NULL)
		{
			printf("malloc client node failed.\n");
			exit(-1);
		}
		memset(client_node_info, 0, sizeof(client_info));
		client_node_info->client_fd = client_fd;
		client_node_info->next = client_list;
		client_list = client_node_info;
		client_num++;
		
		/* 创建事物处理线程，一个客户端对应一个线程 */
		pthread_create(&client_pthread_fd, NULL, client_deal_thread, &client_fd);

	}
	return 0;
}
