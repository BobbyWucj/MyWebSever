#include "web_server/web_server.h"

int main(int argc, char const *argv[])
{
    WebServer server;

    server.initThreadpool();

    server.eventListen();

    server.eventLoop();

    return 0;
}
