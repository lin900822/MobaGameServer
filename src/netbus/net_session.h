#ifndef __NET_SESSION_H__
#define __NET_SESSION_H__

#include "session.h"
#include "uv.h"
#define RECV_LEN 4096

enum class socket_type
{
    TCP_SOCKET,
    WS_SOCKET,
};

class net_session : session
{
  public:
    uv_tcp_t tcp_handle;
    char client_address[32];
    int client_port;

    uv_shutdown_t shutdown_handle;
    uv_write_t write_req;
    uv_buf_t write_buf;

  public:
    char recv_buf[RECV_LEN];
    int recved_len;
    int socket_type;

  private:
    void init();
    void exit();

  public:
    static net_session *create();
    static void destroy(net_session *session);

    // Implementation
  public:
    virtual void close();
    virtual void send_data(unsigned char *body, int len);
    virtual const char *get_address(int *client_port);
};

#endif
