#include "sliding_window.h"
#include <algorithm>
using namespace std;

SlidingWindow::SlidingWindow(int buffSize, int windowSize)
{
    buffSize_ = buffSize;
    sw_.reserve(buffSize);
    //windowSize_ = windowSize;
    start_ = 0;
    end_ = windowSize;
    start_seq_ = 0;
    end_seq_ = 0;
    next_ = 0;
    loss_ack_ = new set<uint32_t>();
}

void SlidingWindow::movePos(slidingPos p, int size)
{
    // lock_guard<mutex> lock(mutex_);
    switch(p)
    {
    case S_START:
        start_ += size;
        start_seq_ += size;
        if(start_ >= buffSize_)
            start_ -= buffSize_;
        break;
    case S_NEXT:
        next_ += size;
        if(next_ >= buffSize_)
            next_ -= buffSize_;
        break;
    case S_END:
        end_ += size;
        if(end_ >= buffSize_)
            end_ -= buffSize_;
        break;
    default:
        break;
    }
    return;
}

void SlidingWindow::setPos(slidingPos p, int pos)
{
    // lock_guard<mutex> lock(mutex_);
    switch(p)
    {
    case S_START:
        start_ = pos;
        start_seq_ = sw_[pos].head.seq;
        if(start_ >= buffSize_)
            start_ -= buffSize_;
        break;
    case S_NEXT:
        next_ = pos;
        if(next_ >= buffSize_)
            next_ -= buffSize_;
        break;
    case S_END:
        end_ = pos;
        if(end_ >= buffSize_)
            end_ -= buffSize_;
        break;
    default:
        break;
    }
}

uint32_t SlidingWindow::getIndexBySeq(uint32_t seq)
{
    // lock_guard<mutex> lock(mutex_);
    return (seq - seq_index_gap_) % buffSize_;
}

uint32_t SlidingWindow::getSeqByIndex(uint32_t index)
{
    // lock_guard<mutex> lock(mutex_);
    return index + seq_index_gap_;
}

uint32_t SlidingWindow::getNext()
{
    return next_;
}

uint32_t SlidingWindow::getNextSeq()
{
    // lock_guard<mutex> lock(mutex_);
    return next_ + seq_index_gap_;
}

uint32_t SlidingWindow::getNextAck()
{
    if(loss_ack_.load()->empty())
        return getSeqByIndex(next_);
    else
        return getSeqByIndex(*(loss_ack_.load()->begin()));
}

uint32_t SlidingWindow::getNextSend()
{
    if(loss_ack_.load()->empty())
        return next_;
    else
        return *(loss_ack_.load()->begin());
}

void SlidingWindow::updateNext(uint32_t index)
{
    if(index == next_)
        movePos(S_NEXT, 1);
    else
    {
        loss_ack_.load()->erase(index);
    }
}

void SlidingWindow::updateMsg(fileMessage* msg)
{
    uint32_t& seq = msg->head.seq;
    if(seq == getNextSeq())
    {
        memcpy(&sw_[next_], msg, sizeof(fileMessage));
        next_ += 1;
        end_ += 1;
    }
    else if(loss_ack_.load()->find(getIndexBySeq(seq)) != loss_ack_.load()->end())
    {
        uint32_t index = getIndexBySeq(seq);
        memcpy(&sw_[index], msg, sizeof(fileMessage));
        loss_ack_.load()->erase(index);
    }
    else if(seq > getNextSeq())
    {
        cout << "[ loss ] : ";
        for(uint32_t i = next_; i != getIndexBySeq(seq); i++)
        {
            cout << getSeqByIndex(i) << " "; 
            loss_ack_.load()->insert(i);
        }
        cout << endl;
        next_ = getIndexBySeq(seq);
        end_ = next_ + SEND_WINDOW_SIZE;
        memcpy(&sw_[next_], msg, sizeof(fileMessage));
        next_ += 1;
        end_ += 1;
    }
}

void SlidingWindow::updateStart()
{
    if(loss_ack_.load()->empty())
        start_ = next_.load();
    else
        start_ = *(loss_ack_.load()->begin());
}

// void SlidingWindow::updateNext()
// {
//     loss_ack_.load()->erase(std::remove(loss_ack_.load()->begin(), loss_ack_.load()->end(), index), loss_ack_.load()->end());
// }

void SlidingWindow::addLossAck(uint32_t ack)
{
    loss_ack_.load()->insert(ack);
}

void SlidingWindow::setData(int index, char* msg, int len)
{
    int pos = index % buffSize_;
    memcpy(sw_[pos].msg, msg, len);
}

void SlidingWindow::setFlag(int index, WORD flag)
{
    int pos = index % buffSize_;
    sw_[pos].head.flag = flag;
}

void SlidingWindow::setWindow(int size)
{
    // lock_guard<mutex> lock(mutex_);
    uint32_t tmp = (start_ + size) % buffSize_;
    uint32_t dif = end_ - tmp;
    if(dif > SEND_WINDOW_SIZE)
        end_ = tmp;
}

int SlidingWindow::getSendWindow()
{
    // lock_guard<mutex> lock(mutex_);
    uint32_t dif = end_ - getNextSend();
    return dif;
}

int SlidingWindow::getWindow()
{
    // lock_guard<mutex> lock(mutex_);
    uint32_t dif = end_ - next_;
    return dif;
}

void SlidingWindow::printSliding()
{
    // lock_guard<mutex> lock(mutex_);
    cout << dec << " satrt : " << start_
    << " next : " << next_
    << " end : " << end_ 
    << " start seq : " << start_seq_ << endl;
}

void SlidingWindow::printAckQuene()
{
    cout << "ack loss : ";
    for(auto i : *loss_ack_.load())
    {
        cout << getSeqByIndex(i) << " ";
    }
    cout << "next : " << getSeqByIndex(next_) << endl;
}

SlidingWindow::~SlidingWindow()
{

}