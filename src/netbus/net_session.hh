#ifndef __NET_SESSION_H__
#define __NET_SESSION_H__

#include "session.hh"
#include "uv.h"

#define RECV_LEN 4096

enum class socket_type
{
    TCP_SOCKET,
    WS_SOCKET,
};

void init_session_allocer();

class net_session : session
{
  public:
    uv_tcp_t tcp_handle;
    char client_address[32];
    int client_port;

    uv_shutdown_t shutdown_handle;
    bool is_shutdown;

  public:
    char recv_buf[RECV_LEN];
    int recved_len;
    int socket_type;

    char *long_pkg;
    int long_pkg_size;

  public:
    int is_ws_handshake;

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
