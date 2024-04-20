#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "net_session.h"
#include "uv.h"

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

#pragma region SessionPool

net_session *net_session::create()
{
    net_session *session = new net_session(); // temp
    session->init();

    return session;
}

void net_session::destroy(net_session *session)
{
    session->exit();

    delete session; // temp;
}

void net_session::init()
{
    memset(this->client_address, 0, sizeof(this->client_address));
    this->client_port = 0;
    this->recved_len = 0;
}

void net_session::exit()
{
    printf("Net Session Exited!\n");
}

#pragma endregion SessionPool

#pragma region Implemetation

void net_session::close()
{
    uv_shutdown_t *req = &this->shutdown_handle;
    memset(req, 0, sizeof(uv_shutdown_t));

    uv_shutdown(req, (uv_stream_t *)&this->tcp_handle, on_shutdown);
}

void net_session::send_data(unsigned char *body, int len)
{
    uv_write_t *write_req = &this->write_req;
    uv_buf_t *write_buf = &this->write_buf;

    *write_buf = uv_buf_init((char *)body, len);
    uv_write(write_req, (uv_stream_t *)&this->tcp_handle, write_buf, 1, on_after_write);
}

const char *net_session::get_address(int *port)
{
    *port = this->client_port;
    return this->client_address;
}

#pragma endregion Implemetation
