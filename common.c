#include "common.h"


uint32_t get_msg_len(uint32_t value_len)
{
	return value_len + sizeof(command_msg);
}
