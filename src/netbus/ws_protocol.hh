#ifndef __WS_PROTOCOL_H__
#define __WS_PROTOCOL_H__

class session;
class ws_protocol
{
  public:
    static bool ws_handshake(session *session, char *body, int len);
    static bool ws_read_header(unsigned char *pkg_data, int pkg_len, int *pkg_size, int *out_header_size);
    static void ws_parser_recv_data(unsigned char *raw_data, unsigned char *mask, int raw_len);
    static unsigned char *ws_package_send_data(const unsigned char *raw_data, int len, int *ws_data_len);
    static void ws_free_send_pkg(unsigned char *ws_pkg);
};

#endif
