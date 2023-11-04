#include "sliding_window.h"
using namespace std;

SlidingWindow::SlidingWindow(int buffSize, int windowSize)
{
    sw_.reserve(buffSize);
    windowSize_ = windowSize;
    start_ = 0;
    end_ = 0;
    start_seq_ = 0;
    end_seq_ = 0;
    next_ = -1;
}

void SlidingWindow::movePos(slidingPos p, int size)
{
    mutex_.lock();
    switch(p)
    {
        case S_START:
            start_ += size;
            break;
        case S_NEXT:
            next_ += size;
            break;
        case S_END:
            end_ += size;
            break;
        default:
            break;
    }
    mutex_.unlock();
    return;
}

SlidingWindow::~SlidingWindow()
{

}