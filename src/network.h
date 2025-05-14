#ifndef NETWORK_H
#define NETWORK_H

#include <stdint.h>

// 初始化网络
void init_network(void);

// 清理网络资源
void cleanup_network(void);

// 启动网络连接
void start_network(void);

// 停止网络连接
void stop_network(void);

// 发送数据到服务器
void send_data_to_server(const void* data, size_t len);

// 网络事件回调函数类型
typedef void (*NetworkCallback)(uint8_t* data, size_t len, void* user_data);

// 设置网络消息回调
void set_network_message_callback(NetworkCallback callback, void* user_data);

#endif // NETWORK_H