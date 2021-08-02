#ifndef SHMVIDEOWRITER_H
#define SHMVIDEOWRITER_H

#ifdef __GNUC__
// The underlying jsoncons headers have some build issues on gcc
// If jsoncons is updated, ensure to fix undef issues in compiler_support.hpp with something like:
// #elif _MSC_VER ->
// #elif defined(_MSC_VER) && _MSC_VER 
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Woverloaded-virtual"
#endif

#include "jsoncons/json.hpp"
#include "jsoncons_ext/bson/bson.hpp"

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

#include <unordered_map>
#include <list>
#include <unistd.h>
#include "shmbuffer.h"
#include <mutex>
#include "semaphore.h"
#include <thread>

using jsoncons::json;
using jsoncons::ojson;
namespace bson = jsoncons::bson;

enum ShmVideoWriter_MsgType : int {
    FRAME_ALLOCATE = 0,
    FRAME_FREE = 1,
    FRAME_STATE_CHANGE = 2,
};

enum ShmVideoWriter_FrameStateType : int {
    FRAME_ADDED = 0,
    FRAME_REMOVED = 1,
    FRAME_CAN_READ = 2,
    FRAME_CAN_WRITE = 3
};

struct ShmVideoWriter_FrameAllocate {
    int32_t width;
    int32_t height;
    int32_t frame_count;
};
JSONCONS_ALL_MEMBER_TRAITS(ShmVideoWriter_FrameAllocate, width, height, frame_count)

struct ShmVideoWriter_FrameFree {
    int32_t frame_id;
};
JSONCONS_ALL_MEMBER_TRAITS(ShmVideoWriter_FrameFree, frame_id)

struct ShmVideoWriter_FrameStateChange {
    int32_t frame_id;
    int32_t width;
    int32_t height;
    int32_t type;
    std::string shmem_path;
};
JSONCONS_ALL_MEMBER_TRAITS(ShmVideoWriter_FrameStateChange, frame_id, width, height, type, shmem_path)

class ShmVideoWriter
{
public:
    ShmVideoWriter();
    void pushIndex(uint32_t index);
    shmbuf* popFrame();
    void sendMessage(ShmVideoWriter_MsgType msgType, uint32_t msg_size, uint8_t* buffer);
    void sendFreeFrame(int32_t index);
    void sendFrameStateChange(shmbuf* frame_buffer, ShmVideoWriter_FrameStateType newState);
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
