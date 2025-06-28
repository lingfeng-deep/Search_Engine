#include "TcpConnection.h"
#include "EventLoop.h"
#include <iostream>
#include <sstream>

using std::cout;
using std::endl;
using std::ostringstream;

TcpConnection::TcpConnection(int fd, EventLoop *loop)
: _loop(loop)
, _sockIO(fd)
, _sock(fd)
, _localAddr(getLocalAddr())
, _peerAddr(getPeerAddr())
{

}

TcpConnection::~TcpConnection()
{

}

void TcpConnection::send(const Message &msg)
{
    /* _sockIO.writen(msg.c_str(), msg.size()); */
    _sockIO.writeTrain(msg);
}

//该函数到底该怎么实现，需要思考一下？
//我们知道需要将数据msg传递给EventLoop
//需要将线程池处理好之后的数据_msg通过TcpConnection的对象发送
//给Reactor，但是Reactor本身没有发送数据的能力,其次接受数据
//与发送数据应该是同一个连接对象,也就是调用sendInLoop函数的
//对象，就是这个连接，这个对象不就是this,除此之外，有了连接
//有了消息，还要有发送数据的函数
void TcpConnection::sendInLoop(const Message &msg)
{
    if(_loop)
    {
        _loop->runInLoop(bind(&TcpConnection::send, this, msg));
    }
#if 0 
    function<void()> f = bind(&TcpConnection::send, this, msg);
    if(_loop)
    {
        //void(function<void()> &&)
        _loop->runInLoop(std::move(f));
    }
#endif
}
string TcpConnection::receive()
{
    char buff[65535] = {0};
    _sockIO.readLine(buff, sizeof(buff));

    return string(buff);
}

Message TcpConnection::receiveTrain()
{
    Message message;
    _sockIO.readTrain(message);

    return message;
}

//判断连接是否断开（被动断开）
bool TcpConnection::isClosed() const
{
    char buff[10] = {0};
    int ret = recv(_sock.fd(), buff, sizeof(buff), MSG_PEEK);

    return 0 == ret;
}
string TcpConnection::toString()
{
    ostringstream oss;
    oss << _localAddr.ip() << ":"
        << _localAddr.port() << "---->"
        << _peerAddr.ip() << ":"
        << _peerAddr.port();

    return oss.str();
}

//获取本端的网络地址信息
InetAddress TcpConnection::getLocalAddr()
{
    struct sockaddr_in addr;
    socklen_t len = sizeof(struct sockaddr );
    //获取本端地址的函数getsockname
    int ret = getsockname(_sock.fd(), (struct sockaddr *)&addr, &len);
    if(-1 == ret)
    {
        perror("getsockname");
    }

    return InetAddress(addr);
}

//获取对端的网络地址信息
InetAddress TcpConnection::getPeerAddr()
{
    struct sockaddr_in addr;
    socklen_t len = sizeof(struct sockaddr );
    //获取对端地址的函数getpeername
    int ret = getpeername(_sock.fd(), (struct sockaddr *)&addr, &len);
    if(-1 == ret)
    {
        perror("getpeername");
    }

    return InetAddress(addr);
}

//注册三个回调
void TcpConnection::setNewConnectionCallback(const TcpConnectionCallback &cb)
{
    _onNewConnection = cb;
}

void TcpConnection::setMessageCallback(const TcpConnectionCallback &cb)
{
    _onMessage = cb;
}

void TcpConnection::setCloseCallback(const TcpConnectionCallback &cb)
{
    _onClose = cb;
}

//执行三个回调
void TcpConnection::handleNewConnectionCallback()
{
    if(_onNewConnection)
    {
        //为了防止智能指针的误用，就不能使用不用的智能指针
        //去托管同一个裸指针
        /* _onNewConnection(shared_ptr<TcpConnection>(this)); */
        _onNewConnection(shared_from_this());
    }
    else
    {
        cout << "_onNewConnection == nullptr" << endl;
    }
}

void TcpConnection::handleMessageCallback()
{
    if(_onMessage)
    {
        _onMessage(shared_from_this());
    }
    else
    {
        cout << "_onMessage == nullptr" << endl;
    }
}

void TcpConnection::handleCloseCallback()
{
    if(_onClose)
    {
        _onClose(shared_from_this());
    }
    else
    {
        cout << "_onClose == nullptr" << endl;
    }
}
