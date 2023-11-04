#ifndef __SLIDING_WINDOW__
#define __SLIDING_WINDOW__

#include <vector>
#include "defs.h"
#include <mutex>

class SlidingWindow
{
private:
    std::vector<fileMessage> sw_;
    int windowSize_;
    volatile int start_;
    int start_seq_;
    int next_;
    int end_;
    int end_seq_;
    std::mutex mutex_;

public:
    SlidingWindow(int buffSize, int windowSize);
    void movePos(slidingPos p, int size);
    ~SlidingWindow();
};

#endif