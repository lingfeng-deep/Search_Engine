# 设置CMake的最小版本
cmake_minimum_required(VERSION 3.15)

# 项目名称
#   项目可以包含多个目标(target)
#   target: 可执行程序, 静态库, 动态库
project(SearchEngine)

# 指定C++的版本: -std=c++17
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 设置可执行程序生成的路径
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ../bin)

# 生成的可执行程序
add_executable(dict_index
    ./src/DictProducer.cpp
    ./src/dict_index.cc
)

# 设置编译选项
target_compile_options(dict_index PRIVATE
    -g
)

# -lcrypto -lssl -ljwt -lwfrest
# target_link_libraries(dict_index PRIVATE 
# )

# 生成的可执行程序
add_executable(page_process
    ./src/page_process.cc
    ./src/tinyxml2.cpp
    ./src/PageProcessor.cpp
)

# 设置编译选项
target_compile_options(page_process PRIVATE
    -g
)

# -lcrypto -lssl -ljwt -lwfrest
# target_link_libraries(page_process PRIVATE 
# )

# 生成的可执行程序
add_executable(client
    ./src/client.cc
)

# 设置编译选项
target_compile_options(client PRIVATE
    -g
)

# -lcrypto -lssl -ljwt -lwfrest
# target_link_libraries(client PRIVATE 
# )

# 生成的可执行程序
add_executable(server
    ./src/reactor/Acceptor.cc
    ./src/reactor/EventLoop.cc
    ./src/reactor/Socket.cc
    ./src/reactor/SocketIO.cc
    ./src/reactor/TaskQueue.cc
    ./src/reactor/TcpConnection.cc
    ./src/reactor/TcpServer.cc
    ./src/reactor/ThreadPool.cc
    ./src/reactor/InetAddress.cc
    ./src/reactor/EchoServer.cc
    ./src/reactor/TestEchoServer.cc
)

# 设置编译选项
target_compile_options(server PRIVATE
    -g
)

# -lcrypto -lssl -ljwt -lwfrest
# target_link_libraries(server PRIVATE 
# )

# 生成的可执行程序
add_executable(http_server
    ./src/SearchServer.cpp
    ./src/http_server.cc
)

# 设置编译选项
target_compile_options(http_server PRIVATE
    -g
)

# -lcrypto -lssl -ljwt -lwfrest
target_link_libraries(http_server PRIVATE 
    wfrest
    redis++
    hiredis
    pthread
)
