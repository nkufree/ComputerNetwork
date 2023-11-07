#ifndef __SLIDING_WINDOW__
#define __SLIDING_WINDOW__

#include <vector>
#include "defs.h"
#include <mutex>
#define GET_POINT(p) \
    mutex_.lock(); \
    uint32_t tmp = p; \
    mutex_.unlock(); \
    return tmp;

class SlidingWindow
{
private:
    int buffSize_;
    //int windowSize_;
    volatile uint32_t start_;
    volatile uint32_t start_seq_;
    volatile uint32_t next_;
    volatile uint32_t end_;
    volatile uint32_t end_seq_;
    uint32_t data_end_;
    std::mutex mutex_;

public:
    std::vector<fileMessage> sw_;
    SlidingWindow(int buffSize, int windowSize);
    SlidingWindow(){}
    void movePos(slidingPos p, int size);
    void setPos(slidingPos p, int pos);
    int getWindow();
    void setWindow(int size);
    void setStartSeq(int seq){start_seq_ = seq;}
    uint32_t getStartSeq() {GET_POINT(start_seq_);}
    uint32_t getStart() {GET_POINT(start_);}
    uint32_t getNext() {GET_POINT(next_);}
    uint32_t getEnd() {GET_POINT(end_);}
    uint32_t getDataEnd(){return data_end_;}
    uint32_t getIndexBySeq(uint32_t seq);
    uint32_t getSeqByIndex(uint32_t index);
    uint32_t getNextSeq();
    void setData(int index, char* msg, int len);
    void setFlag(int index, WORD flag);
    void setDataEnd(int pos) {data_end_ = pos % buffSize_;}
    void printSliding();
    ~SlidingWindow();
};

#endif