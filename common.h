#ifndef _COMMON_H_
#define _COMMON_H_
#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <errno.h>

#define PORT 9999
#define MAX_SOCKET_BUF 2048


typedef struct _client_info{
	uint32_t client_fd;
	struct _client_info* next;
}client_info;

typedef enum{
	LIST = 0,
	LIST_ACK,
	USER_MSG,
	MSG_ACK,
}command_type;

typedef struct _command_msg{
	uint32_t src_fd;
	uint32_t dst_fd;
	command_type type;
	uint16_t len;
	uint8_t value[0];
}command_msg;

typedef struct _usr_command{
	uint8_t command_seq;
	uint8_t command_name[20];
	uint8_t	command_desc[50];
	void (*command_fun)(uint8_t* arg);
}usr_command;

uint32_t get_msg_len(uint32_t value_len);

#endif
