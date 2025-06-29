#include "ws_protocol.hh"
#include "http_parser.h"
#include "session.hh"

#include "../../3rd/crypto/base64_encoder.h"
#include "../../3rd/crypto/sha1.h"
#include "../../3rd/http_parser/http_parser.h"

#include "../utils/cache_alloc.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>

extern cache_allocer *wbuf_allocer;

// base64(sha1(key + ws_magic))
static char *ws_magic = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
static char *ws_accept = "HTTP/1.1 101 Switching Protocols\r\n"
                         "Upgrade:websocket\r\n"
                         "Connection: Upgrade\r\n"
                         "Sec-WebSocket-Accept: %s\r\n"
                         "WebSocket-Protocol:chat\r\n\r\n";

static char filed_sec_key[512];
static char value_sec_key[512];
static int is_sec_key = 0;
static int has_sec_key = 0;
static int is_shake_ended = 0;

extern "C"
{
    static int on_message_end(http_parser *parser)
    {
        is_shake_ended = 1;
        return 0;
    }

    static int on_ws_header_field(http_parser *parser, const char *at, size_t length)
    {
        if (strncmp(at, "Sec-WebSocket-Key", length) == 0)
        {
            is_sec_key = 1;
        }
        else
        {
            is_sec_key = 0;
        }
        return 0;
    }

    static int on_ws_header_value(http_parser *parser, const char *at, size_t length)
    {
        if (!is_sec_key)
        {
            return 0;
        }

        strncpy(value_sec_key, at, length);
        value_sec_key[length] = 0;
        has_sec_key = 1;

        return 0;
    }
}

bool ws_protocol::ws_handshake(session *session, char *body, int len)
{
    http_parser_settings settings;
    http_parser_settings_init(&settings);

    settings.on_header_field = on_ws_header_field;
    settings.on_header_value = on_ws_header_value;
    settings.on_message_complete = on_message_end;

    http_parser parser;
    http_parser_init(&parser, HTTP_REQUEST);

    is_sec_key = 0;
    has_sec_key = 0;
    is_shake_ended = 0;
    http_parser_execute(&parser, &settings, body, len);

    if (has_sec_key && is_shake_ended)
    {
        printf("Sec-WebSocket-Key: %s\n", value_sec_key);
        // key + magic
        static char key_magic[512];
        static char sha1_key_magic[SHA1_DIGEST_SIZE];
        static char send_client[512];

        int sha1_size;

        sprintf(key_magic, "%s%s", value_sec_key, ws_magic);
        crypt_sha1((unsigned char *)key_magic, strlen(key_magic), (unsigned char *)&sha1_key_magic, &sha1_size);
        int base64_len;
        char *base_buf = base64_encode((uint8_t *)sha1_key_magic, sha1_size, &base64_len);
        sprintf(send_client, ws_accept, base_buf);
        base64_encode_free(base_buf);

        session->send_data((unsigned char *)send_client, strlen(send_client));
        return true;
    }
    return false;
}

bool ws_protocol::ws_read_header(unsigned char *recv_data, int recv_len, int *pkg_size, int *out_header_size)
{
    if (recv_data[0] != 0x81 && recv_data[0] != 0x82)
    {
        return false;
    }

    if (recv_len < 2)
    {
        return false;
    }

    unsigned int data_len = recv_data[1] & 0x0000007f; // 取得Payload len (Mask 0111 1111)
    int head_size = 2;
    if (data_len == 126) // 判斷Payload len是否需要擴充
    {
        head_size += 2;
        if (recv_len < head_size)
        {
            return false;
        }
        data_len = recv_data[3] | (recv_data[2] << 8);
    }
    else if (data_len == 127)
    {
        head_size += 8;
        if (recv_len < head_size)
        {
            return false;
        }

        unsigned int low = recv_data[5] | (recv_data[4] << 8) | (recv_data[3] << 16) | (recv_data[2] << 24);
        unsigned int high = recv_data[9] | (recv_data[8] << 8) | (recv_data[7] << 16) | (recv_data[6] << 24);
        data_len = low;
    }

    head_size += 4; // 4 个mask
    *pkg_size = data_len + head_size;
    *out_header_size = head_size;

    return true;
}

void ws_protocol::ws_parser_recv_data(unsigned char *raw_data, unsigned char *mask, int raw_len)
{
    for (int i = 0; i < raw_len; i++)
    {
        raw_data[i] = raw_data[i] ^ mask[i % 4];
    }
}

unsigned char *ws_protocol::ws_package_send_data(const unsigned char *raw_data, int len, int *ws_data_len)
{
    int head_size = 2;
    if (len > 125 && len < 65536)
    {
        head_size += 2;
    }
    else if (len >= 65536)
    {
        head_size += 8;
        return nullptr;
    }

    unsigned char *data_buf = (unsigned char *)cache_alloc(wbuf_allocer, head_size + len);

    data_buf[0] = 0x81; // 0x81 字串 0x82 二進位
    if (len <= 125)
    {
        data_buf[1] = len;
    }
    else if (len > 125 && len < 65536)
    {
        data_buf[1] = 126;
        data_buf[2] = (len & 0x0000ff00) >> 8;
        data_buf[3] = (len & 0x000000ff);
    }

    memcpy(data_buf + head_size, raw_data, len);
    *ws_data_len = (head_size + len);

    return data_buf;
}

void ws_protocol::ws_free_send_pkg(unsigned char *ws_pkg)
{
    cache_free(wbuf_allocer, ws_pkg);
}
