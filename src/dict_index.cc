#include "../include/DictProducer.h"

int main(int argc, char *argv[])
{
    SplitToolCppJieba splitTool;
    DictProducer en_dp("../corpus/EN/");
    DictProducer cn_dp("../corpus/CN/", &splitTool);
    return 0;
}

