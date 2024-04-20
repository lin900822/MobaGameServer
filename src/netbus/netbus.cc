#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "net_session.h"
#include "uv.h"

#include "netbus.h"

extern "C"
{
    static void on_client_connected(uv_stream_t *server, int status);
    static void on_alloc_buf(uv_handle_t *handle, size_t suggested_size, uv_buf_t *buf);
    static void on_after_read(uv_stream_t *stream, ssize_t nread, const uv_buf_t *buf);

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
        net_session *session = (net_session *)handle->data;
        *buf = uv_buf_init(session->recv_buf + session->recved_len, RECV_LEN - session->recved_len);
    }

    static void on_after_read(uv_stream_t *stream, ssize_t nread, const uv_buf_t *buf)
    {
        net_session *session = (net_session *)stream->data;
        if (nread < 0)
        {
            session->close();
            return;
        }

        buf->base[nread] = 0;
        printf("recv %d\n", nread);
        printf("%s\n", buf->base);

        session->send_data((unsigned char *)buf->base, nread);
        session->recved_len = 0;
    }
}

static netbus g_netbus;
netbus *netbus::instance()
{
    return &g_netbus;
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

void netbus::run()
{
    uv_run(uv_default_loop(), UV_RUN_DEFAULT);
}