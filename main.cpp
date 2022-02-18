#include "web_server/web_server.h"

int main()
{
    WebServer server;

    server.initThreadpool();

    server.eventListen();

    server.eventLoop();

    return 0;
}
