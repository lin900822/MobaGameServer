#include <new>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../utils/cache_alloc.h"
#include "net_session.h"
#include "uv.h"

#include "ws_protocol.h"

#define SESSION_CACHE_CAPACITY 6000
#define WRITE_REQ_CACHE_CAPCITY 4096

static cache_allocer *session_allocer = nullptr;
static cache_allocer *write_req_allocer = nullptr;

void init_session_allocer()
{
    if (session_allocer == nullptr)
    {
        session_allocer = create_cache_allocer(SESSION_CACHE_CAPACITY, sizeof(net_session));
    }

    if (write_req_allocer == nullptr)
    {
        write_req_allocer = create_cache_allocer(WRITE_REQ_CACHE_CAPCITY, sizeof(uv_write_t));
    }
}

extern "C"
{
    static void on_after_write(uv_write_t *req, int status);
    static void on_shutdown(uv_shutdown_t *req, int status);
    static void on_close(uv_handle_t *handle);

    static void on_after_write(uv_write_t *req, int status)
    {
        if (status == 0)
        {
            printf("write success\n");
        }
        cache_free(write_req_allocer, req);
    }

    static void on_shutdown(uv_shutdown_t *req, int status)
    {
        uv_close((uv_handle_t *)req->handle, on_close);
    }

    static void on_close(uv_handle_t *handle)
    {
        net_session *session = (net_session *)handle->data;
        net_session::destroy(session);
    }
}

#pragma region Cache

net_session *net_session::create()
{
    net_session *session = (net_session *)cache_alloc(session_allocer, sizeof(net_session));
    session = new (session) net_session(); // 手動Call 建構子
    session->init();

    return session;
}

void net_session::destroy(net_session *session)
{
    session->exit();

    session->~net_session(); // 手動Call 解構子
    cache_free(session_allocer, session);
}

void net_session::init()
{
    memset(this->client_address, 0, sizeof(this->client_address));
    this->client_port = 0;
    this->recved_len = 0;
    this->is_shutdown = false;
    this->is_ws_handshake = 0;
    this->long_pkg = nullptr;
    this->long_pkg_size = 0;
}

void net_session::exit()
{
    printf("Net Session Exited!\n");
}

#pragma endregion Cache

#pragma region Implemetation

void net_session::close()
{
    if (this->is_shutdown)
    {
        return;
    }
    this->is_shutdown = true;

    uv_shutdown_t *req = &this->shutdown_handle;
    memset(req, 0, sizeof(uv_shutdown_t));

    uv_shutdown(req, (uv_stream_t *)&this->tcp_handle, on_shutdown);
}

void net_session::send_data(unsigned char *body, int len)
{
    uv_write_t *write_req = (uv_write_t *)cache_alloc(write_req_allocer, sizeof(uv_write_t));
    uv_buf_t write_buf;

    if (this->socket_type == (int)socket_type::WS_SOCKET && this->is_ws_handshake)
    {
        int ws_pkg_len;
        unsigned char *ws_pkg = ws_protocol::ws_package_send_data(body, len, &ws_pkg_len);
        write_buf = uv_buf_init((char *)ws_pkg, ws_pkg_len);
        uv_write(write_req, (uv_stream_t *)&this->tcp_handle, &write_buf, 1, on_after_write);
        ws_protocol::ws_free_send_pkg(ws_pkg);
    }
    else
    {
        write_buf = uv_buf_init((char *)body, len);
        uv_write(write_req, (uv_stream_t *)&this->tcp_handle, &write_buf, 1, on_after_write);
    }
}

const char *net_session::get_address(int *port)
{
    *port = this->client_port;
    return this->client_address;
}

#pragma endregion Implemetation
