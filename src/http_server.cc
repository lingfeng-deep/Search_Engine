#include "../include/SearchServer.h"
#include <signal.h>
#include <iostream>

WFFacilities::WaitGroup g_waitGroup { 1 };

void sig_handler(int)
{
    g_waitGroup.done();
}

int main()
{

    signal(SIGINT, sig_handler);

    SearchServer svr;

    // 注册路由
    svr.register_modules();

    //svr.start(8888);
    if (svr.track().start(8888) == 0) {
        svr.start_timer();
        svr.list_routes();
        g_waitGroup.wait();
        svr.stop();
    } else {
        std::cerr << "Error: server start failed!\n";
    }
}
