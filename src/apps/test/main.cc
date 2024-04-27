#include <stdio.h>
#include <stdlib.h>

#include "../../netbus/netbus.h"

// 02 00:27:37
int main(int argc, char **argv)
{
    printf("Moba Game Server!\n");

    netbus::instance()->init();

    netbus::instance()->start_tcp_server(9100);
    netbus::instance()->start_ws_server(8001);

    netbus::instance()->run();

    return 0;
}