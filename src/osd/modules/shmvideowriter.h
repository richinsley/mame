#ifndef SHMVIDEOWRITER_H
#define SHMVIDEOWRITER_H

#include <unordered_map>
#include <list>
#include <unistd.h>
#include "shmbuffer.h"
#include <mutex>
#include "semaphore.h"
#include "mamecast.pb.h"
#include <thread>

class ShmVideoWriter
{
public:
    ShmVideoWriter();
    void pushIndex(uint32_t index);
    shmbuf* popFrame();
    void sendMessage(mamecast_protocol::MsgType msgType, uint32_t msg_size, uint8_t* buffer);
    void sendFreeFrame(int32_t index);
    void sendFrameStateChange(shmbuf* frame_buffer, mamecast_protocol::FrameStateChange_FrameStateType newState);
    void requestFrames(int count, int width, int height);
    void start();
    void quit();
private:
    void doVideo();

    std::unordered_map<uint32_t, shmbuf*>   _frames;
    std::list<uint32_t>                     _available_indexes;
    sem_t                                   _frame_sem;
    std::mutex                              _frame_mtx;
    bool                                    _exitVidThread;
    std::thread*                            _threadObj;
};

#endif // SHMVIDEOWRITER_H
