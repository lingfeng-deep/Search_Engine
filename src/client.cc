#include <iostream>
#include <string>
#include <vector>
#include <my_header.h>

struct Message{
    int tag;
    int length;
    std::string value;
};


int tcp_connect(const char* host, const char* service) {
    int socket_fd = socket(AF_INET, SOCK_STREAM, 0);                                             
    ERROR_CHECK(socket_fd, -1, "socket");

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(atoi(service));
    addr.sin_addr.s_addr = inet_addr(host);

    int connect_ret = connect(socket_fd, (struct sockaddr *)&addr, sizeof(addr));
    ERROR_CHECK(connect_ret, -1, "connect");
    return socket_fd;
}

int writen(int connfd, const char *buf, int len)
{
    int left = len;
    const char *pstr = buf;
    int ret = 0;

    while(left > 0)
    {
        ret = write(connfd, pstr, left);
        if(-1 == ret && errno == EINTR)
        {
            continue;
        }
        else if(-1 == ret)
        {
            perror("writen error -1");
            return -1;
        }
        else if(0 == ret)
        {
            break;
        }
        else
        {
            pstr += ret;
            left -= ret;
        }
    }
    return len - left;
}

void send_message(int connfd, const Message& msg)
{
    int tag = msg.tag;
    int length = msg.length;
    std::string value = msg.value;
    writen(connfd, (char *)&tag, sizeof(tag));
    writen(connfd, (char *)&length, sizeof(length));
    writen(connfd, value.c_str(), length);
}

int readn(int connfd, char *buf, int len)
{
    int left = len;
    char *pstr = buf;
    int ret = 0;

    while(left > 0)
    {
        ret = read(connfd, pstr, left);
        if(-1 == ret && errno == EINTR)
        {
            continue;
        }
        else if(-1 == ret)
        {
            perror("read error -1");
            return -1;
        }
        else if(0 == ret)
        {
            break;
        }
        else
        {
            pstr += ret;
            left -= ret;
        }
    }

    return len - left;
}

void recv_message(int connfd, Message & msg)
{
    readn(connfd, (char *)&msg.tag, sizeof(msg.tag));
    readn(connfd, (char *)&msg.length, sizeof(msg.length));
    // 正确读取字符串内容
    std::vector<char> buffer(msg.length);
    readn(connfd, buffer.data(), msg.length);
    std::string value(buffer.begin(), buffer.end());
    msg.value = value;
}

int main()
{
    const char* host = "127.0.0.1";
    const char* service = "8888";
    int connfd = tcp_connect(host, service); 
    if (connfd == -1) {
        std::cerr << "Error: connnect server failed!\n";
        exit(1);
    }

    for (;;) {
        std::cout << "message: <Tag><Value>\n";
        Message message;
        std::cin >> message.tag;
        /* std::cin >> message.value; */
        getchar();
        std::getline(std::cin, message.value);
        message.length = message.value.size();

        send_message(connfd, message);

        // 接收服务器的响应
        recv_message(connfd, message);
        std::cout << message.value << std::endl;
    }
    close(connfd);
    return 0;
}

