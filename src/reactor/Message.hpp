#pragma once

#include <string>

struct Message {
    int tag;
    int length;
    std::string value;
};
