#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "net_session.hh"
#include "session.hh"
#include "tp_protocol.hh"
#include "uv.h"
#include "ws_protocol.hh"

#include "netbus.hh"

extern "C"
{
    static void on_client_connected(uv_stream_t *server, int status);
    static void on_alloc_buf(uv_handle_t *handle, size_t suggested_size, uv_buf_t *buf);
    static void on_after_read(uv_stream_t *stream, ssize_t nread, const uv_buf_t *buf);
    static void on_recv_ws_data(net_session *sess);
    static void on_recv_tcp_data(net_session *sess);
    static void on_recv_client_cmd(session *sess, unsigned char *body, int len);

    // 當有新的連線接入
    static void on_client_connected(uv_stream_t *server, int status)
    {
        net_session *session = net_session::create();

        uv_tcp_t *client = &session->tcp_handle;
        memset(client, 0, sizeof(uv_tcp_t));

        uv_tcp_init(uv_default_loop(), client);
        client->data = (void *)session;
        uv_accept(server, (uv_stream_t *)client);

        struct sockaddr_in addr;
        int len = sizeof(addr);
        uv_tcp_getpeername(client, (sockaddr *)&addr, &len);
        uv_ip4_name(&addr, (char *)session->client_address, 64);

        session->client_port = ntohs(addr.sin_port);
        session->socket_type = *(int *)(server->data);
        printf("new client comming %s:%d\n", session->client_address, session->client_port);

        uv_read_start((uv_stream_t *)client, on_alloc_buf, on_after_read);
    }

    static void on_alloc_buf(uv_handle_t *handle, size_t suggested_size, uv_buf_t *buf)
    {
        net_session *sess = (net_session *)handle->data;

        // 判斷是否是長封包
        if (sess->recved_len < RECV_LEN)
        {
            *buf = uv_buf_init(sess->recv_buf + sess->recved_len, RECV_LEN - sess->recved_len);
        }
        else
        {
            if (sess->long_pkg == NULL)
            {
                if (sess->socket_type == (int)socket_type::WS_SOCKET && sess->is_ws_handshake)
                {
                    int pkg_size;
                    int head_size;
                    ws_protocol::ws_read_header((unsigned char *)sess->recv_buf, sess->recved_len, &pkg_size, &head_size);
                    sess->long_pkg_size = pkg_size;
                    sess->long_pkg = (char *)malloc(pkg_size);
                    memcpy(sess->long_pkg, sess->recv_buf, sess->recved_len);
                }
                else
                {
                }
            }
            *buf = uv_buf_init(sess->long_pkg + sess->recved_len, sess->long_pkg_size - sess->recved_len);
        }
    }

    static void on_after_read(uv_stream_t *stream, ssize_t nread, const uv_buf_t *buf)
    {
        net_session *sess = (net_session *)stream->data;
        if (nread < 0)
        {
            sess->close();
            return;
        }

        sess->recved_len += nread;

        if (sess->socket_type == (int)socket_type::WS_SOCKET) // Websocket協議
        {
            if (sess->is_ws_handshake == 0) // 還沒握手
            {
                if (ws_protocol::ws_handshake((session *)sess, sess->recv_buf, sess->recved_len))
                {
                    sess->is_ws_handshake = 1;
                    sess->recved_len = 0;
                }
            }
            else // 已握手 正常收發資料
            {
                on_recv_ws_data(sess);
            }
        }
        else // 普通TCP協議
        {
            on_recv_tcp_data(sess);
        }
    }

    static void on_recv_ws_data(net_session *sess)
    {
        unsigned char *pkg_data = (unsigned char *)((sess->long_pkg != nullptr) ? sess->long_pkg : sess->recv_buf);

        while (sess->recved_len > 0)
        {
            int pkg_size = 0;
            int head_size = 0;

            if (pkg_data[0] == 0x88) // 收到斷開連線請求
            {
                sess->close();
                break;
            }

            if (!ws_protocol::ws_read_header(pkg_data, sess->recved_len, &pkg_size, &head_size))
            {
                break;
            }

            if (sess->recved_len < pkg_size)
            {
                break;
            }

            unsigned char *raw_data = pkg_data + head_size;
            unsigned char *mask = raw_data - 4;
            int body_size = pkg_size - head_size;
            ws_protocol::ws_parser_recv_data(raw_data, mask, body_size); // 解 Mask

            on_recv_client_cmd((session *)sess, raw_data, body_size);

            if (sess->recved_len > pkg_size)
            {
                memmove(pkg_data, pkg_data + pkg_size, sess->recved_len - pkg_size);
            }
            sess->recved_len -= pkg_size;

            if (sess->recved_len == 0 && sess->long_pkg != nullptr)
            {
                free(sess->long_pkg);
                sess->long_pkg = nullptr;
                sess->long_pkg_size = 0;
            }
        }
    }

    static void on_recv_tcp_data(net_session *sess)
    {
        unsigned char *pkg_data = (unsigned char *)((sess->long_pkg != nullptr) ? sess->long_pkg : sess->recv_buf);

        while (sess->recved_len > 0)
        {
            int pkg_size = 0;
            int head_size = 0;

            if (!tp_protocol::read_header(pkg_data, sess->recved_len, &pkg_size, &head_size))
            {
                break;
            }

            if (sess->recved_len < pkg_size)
            {
                break;
            }

            unsigned char *raw_data = pkg_data + head_size;
            int body_size = pkg_size - head_size;

            on_recv_client_cmd((session *)sess, raw_data, body_size);

            if (sess->recved_len > pkg_size)
            {
                memmove(pkg_data, pkg_data + pkg_size, sess->recved_len - pkg_size);
            }
            sess->recved_len -= pkg_size;

            if (sess->recved_len == 0 && sess->long_pkg != nullptr)
            {
                free(sess->long_pkg);
                sess->long_pkg = nullptr;
                sess->long_pkg_size = 0;
            }
        }
    }

    static void on_recv_client_cmd(session *sess, unsigned char *body, int len)
    {
        printf("client command!!!\n");

        sess->send_data(body, len);
    }
}

static netbus g_netbus;
netbus *netbus::instance()
{
    return &g_netbus;
}

void netbus::init()
{
    init_session_allocer();
}

void netbus::start_tcp_server(int port)
{
    uv_tcp_t *listen = new uv_tcp_t();

    uv_tcp_init(uv_default_loop(), listen);

    struct sockaddr_in addr;
    uv_ip4_addr("0.0.0.0", port, &addr);

    int ret = uv_tcp_bind(listen, (const struct sockaddr *)&addr, 0);
    if (ret != 0)
    {
        printf("bind error\n");
        delete (listen);
        return;
    }

    uv_listen((uv_stream_t *)listen, SOMAXCONN, on_client_connected);
    listen->data = new int((int)socket_type::TCP_SOCKET);
}

void netbus::start_ws_server(int port)
{
    uv_tcp_t *listen = new uv_tcp_t();

    uv_tcp_init(uv_default_loop(), listen);

    struct sockaddr_in addr;
    uv_ip4_addr("0.0.0.0", port, &addr);

    int ret = uv_tcp_bind(listen, (const struct sockaddr *)&addr, 0);
    if (ret != 0)
    {
        printf("bind error\n");
        delete (listen);
        return;
    }

    uv_listen((uv_stream_t *)listen, SOMAXCONN, on_client_connected);
    listen->data = new int((int)socket_type::WS_SOCKET);
}

void netbus::run()
{
    uv_run(uv_default_loop(), UV_RUN_DEFAULT);
}