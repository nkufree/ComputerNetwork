#ifndef __SLIDING_WINDOW__
#define __SLIDING_WINDOW__

#include <vector>
#include "defs.h"
#include <mutex>
#include <atomic>
#include <set>

class SlidingWindow
{
private:
    int buffSize_;
    //int windowSize_;
    std::atomic<uint32_t> start_;
    std::atomic<uint32_t> start_seq_;
    std::atomic<uint32_t> next_;
    std::atomic<std::set<uint32_t>*> loss_ack_;
    std::atomic<uint32_t> end_;
    std::atomic<uint32_t> end_seq_;
    uint32_t data_end_;
    uint32_t seq_index_gap_;
    // std::mutex mutex_;

public:
    std::vector<fileMessage> sw_;
    SlidingWindow(int buffSize, int windowSize);
    SlidingWindow(){loss_ack_ = new std::set<uint32_t>();}
    void movePos(slidingPos p, int size);
    void setPos(slidingPos p, int pos);
    int getSendWindow();
    int getWindow();
    void setWindow(int size);
    void setStartSeq(int seq){start_seq_ = seq; seq_index_gap_ = start_seq_ - start_;}
    uint32_t getStartSeq() {return start_seq_;}
    uint32_t getStart() {return start_;}
    uint32_t getNext();
    uint32_t getEnd() {return next_;}
    uint32_t getDataEnd(){return data_end_;}
    uint32_t getIndexBySeq(uint32_t seq);
    uint32_t getSeqByIndex(uint32_t index);
    void addLossAck(uint32_t ack);
    int getLossNum(){return loss_ack_.load()->size();}
    uint32_t getNextAck();
    uint32_t getNextSend();
    void updateNext(uint32_t index);
    // void updateNext();
    void updateStart();
    void updateMsg(fileMessage* msg);
    uint32_t getNextSeq();
    void setData(int index, char* msg, int len);
    void setFlag(int index, WORD flag);
    void setDataEnd(int pos) {data_end_ = pos % buffSize_;}
    void printSliding();
    void printAckQuene();
    ~SlidingWindow();
};

#endif