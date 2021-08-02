#include "shmvideowriter.h"
#include "socketpipe.h"
#include "shm_map.h"
#include <arpa/inet.h>

ShmVideoWriter::ShmVideoWriter() :
    _exitVidThread(false),
    _threadObj(nullptr)
{
    sem_init(&_frame_sem, 0, 0);
}

void ShmVideoWriter::pushIndex(uint32_t index) {
    _frame_mtx.lock();
    _available_indexes.push_back(index);
    _frame_mtx.unlock();
    sem_post(&_frame_sem);
}

shmbuf* ShmVideoWriter::popFrame() {
    sem_wait(&_frame_sem);
    _frame_mtx.lock();
    uint32_t index = _available_indexes.front();
    _available_indexes.pop_front();
    shmbuf* retv = _frames[index];
    _frame_mtx.unlock();
    return retv;
}

void ShmVideoWriter::sendMessage(ShmVideoWriter_MsgType msgType, uint32_t msg_size, uint8_t* buffer)
{
    osd_socket_pipe& instance = osd_socket_pipe::Instance();
    int32_t n_mtype = htonl((int32_t)msgType);
    int32_t n_msg_size = htonl((int32_t)msg_size);
    instance.writeVideoBuffer((uint8_t*)&n_mtype, sizeof(n_mtype));
    instance.writeVideoBuffer((uint8_t*)&n_msg_size, sizeof(n_msg_size));
    instance.writeVideoBuffer(buffer, msg_size);
}

void ShmVideoWriter::sendFrameStateChange(shmbuf* frame_buffer, ShmVideoWriter_FrameStateType newState)
{
    ShmVideoWriter_FrameStateChange state;
    state.width = frame_buffer->width;
    state.height = frame_buffer->height;
    state.frame_id = frame_buffer->frame_id;
    state.type = newState;
    state.shmem_path = "";

    std::vector<char> buffer;
    jsoncons::bson::encode_bson(state, buffer);
    sendMessage(ShmVideoWriter_MsgType::FRAME_STATE_CHANGE, buffer.size(), (uint8_t*)buffer.data());
}

void ShmVideoWriter::sendFreeFrame(int32_t index)
{
    ShmVideoWriter_FrameFree ff;
    ff.frame_id = index;
    std::vector<char> buffer;
    jsoncons::bson::encode_bson(ff, buffer);
    sendMessage(ShmVideoWriter_MsgType::FRAME_FREE, buffer.size(), (uint8_t*)buffer.data());
}

void ShmVideoWriter::doVideo()
{
    osd_socket_pipe& instance = osd_socket_pipe::Instance();
    uint8_t intbuffer[4];
    uint8_t msgbuffer[1024];

    bool fin = false;
    while(!fin)
    {
        instance.readVideoBuffer(intbuffer, 4);
        ShmVideoWriter_MsgType t = (ShmVideoWriter_MsgType)ntohl(*(uint32_t*)intbuffer);
        instance.readVideoBuffer(intbuffer, 4);
        int32_t msglen = (int32_t)ntohl(*(uint32_t*)intbuffer);
        instance.readVideoBuffer(msgbuffer, (size_t)msglen);
        switch (t)
        {
            case ShmVideoWriter_MsgType::FRAME_STATE_CHANGE:
                {
                    std::vector<uint8_t> mbuf(msgbuffer, msgbuffer + msglen);
                    auto msg = jsoncons::bson::decode_bson<ShmVideoWriter_FrameStateChange>(mbuf);
                    switch (msg.type) {
                    case ShmVideoWriter_FrameStateType::FRAME_ADDED:
                        {
                            shmbuf* n2 = openShmBuffer(msg.shmem_path.c_str(),
                                                       msg.width * msg.height * 4,
                                                       false,
                                                       msg.frame_id,
                                                       msg.width,
                                                       msg.height);
                            _frame_mtx.lock();
                            _frames[msg.frame_id] = n2;
                            _frame_mtx.unlock();
                            pushIndex(msg.frame_id);
                        }
                        break;
                    case ShmVideoWriter_FrameStateType::FRAME_REMOVED:
                        {
                            _frame_mtx.lock();

                            // remove from the map
                            uint32_t fid = msg.frame_id;
                            shmbuf* buf = _frames[fid];
                            _frames.erase(buf->frame_id);

                            // see if we have it in our stack
                            std::list<uint32_t>::iterator it = std::find(_available_indexes.begin(), _available_indexes.end(), fid);
                            if(it != _available_indexes.end())
                            {
                                // nom our semaphore to indicate we are going to be one frame short
                                sem_wait(&_frame_sem);

                                // remove from the available list
                                _available_indexes.erase(it);
                            }

                            // check if all our frames are accounted for and the map is empty
                            if(!_frames.size())
                            {
                                fin = true;
                            }

                            _frame_mtx.unlock();

                            if(fin)
                            {
                                continue;
                            }
                        }
                        break;
                    case ShmVideoWriter_FrameStateType::FRAME_CAN_WRITE:
                        {
                            // we simply need to push the frame id onto our available stack
                            pushIndex(msg.frame_id);
                        }
                        break;
                    default:
                        break;
                    }
                }
                break;
            // These only happen on the server side
            //case ShmVideoWriter_MsgType::FRAME_ALLOCATE:
            //    break;
            //case ShmVideoWriter_MsgType::FRAME_FREE:
            //    break;
            default:
                break;
        }
    }
}

void ShmVideoWriter::requestFrames(int count, int width, int height)
{
    ShmVideoWriter_FrameAllocate f;
    f.width = width;
    f.height = height;
    f.frame_count = count;

    std::vector<char> buffer;
    jsoncons::bson::encode_bson(f, buffer);
    sendMessage(ShmVideoWriter_MsgType::FRAME_ALLOCATE, buffer.size(), (uint8_t*)buffer.data());
}

void ShmVideoWriter::start()
{
    if(!_threadObj)
    {
        _threadObj = new std::thread(&ShmVideoWriter::doVideo, this);
    }
}

void ShmVideoWriter::quit()
{
    if(_threadObj)
    {
        // set the exit thread flag.  when all frames are released, the frame reader thread will exit
        _exitVidThread = true;

        // release our frames
        for (auto it : _frames)
        {
            uint32_t index = it.first;
            sendFreeFrame(index);
        }

        _threadObj->join();
    }
    delete _threadObj;
    _threadObj = NULL;
}
