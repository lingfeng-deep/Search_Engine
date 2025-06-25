#ifndef __ECHOSERVER_H__
#define __ECHOSERVER_H__

#include "ThreadPool.h"
#include "TcpServer.h"
#include "Message.hpp"

class MyTask
{
public:
    /* MyTask(const string &msg, const TcpConnectionPtr &con); */
    MyTask(const Message &msg, const TcpConnectionPtr &con);
    void process();
private:
    Message _msg;
    TcpConnectionPtr _con;//要保证同一个连接
};

class EchoServer
{
public:
    EchoServer(size_t threadNum, size_t queSize
               , const string &ip
               , unsigned short port);
    ~EchoServer();

    //服务器的启动与停止
    void start();
    void stop();

    //三个回调
    void onNewConnection(const TcpConnectionPtr &con);
    void onMessage(const TcpConnectionPtr &con);
    void onClose(const TcpConnectionPtr &con);

private:
    ThreadPool _pool;//线程池子对象
    TcpServer _server;//TcpServer子对象

};

#endif
