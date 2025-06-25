#ifndef __SOCKETIO_H__
#define __SOCKETIO_H__

#include "Message.hpp"
#include "nlohmann/json.hpp"

using json = nlohmann::json;

class SocketIO
{
public:
    explicit SocketIO(int fd);
    ~SocketIO();
    int readn(char *buf, int len);
    int readLine(char *buf, int len);
    void readTrain(Message & message);
    int writen(const char *buf, int len);
    void writeTrain(const Message & msg);

private:
    int _fd;
};

#endif
