#ifndef __SOCKETPIPE__
#define __SOCKETPIPE__

#include "emu.h"
#include "screen.h"
#include <thread>
#include <semaphore.h>

struct socketpipe_screen
{
    int                 width;
    int                 height;
    int                 pixclock;
    double              refresh_rate;
    screen_type_enum    screen_type;
};

class ISocketPipeDataCB
{
    public:
        virtual void onData(uint8_t * buffer, size_t len) = 0;
};

class osd_socket_pipe {
public:
    // singleton instance getter
    static osd_socket_pipe& Instance();

    // delete copy and move constructors and assign operators
    osd_socket_pipe(osd_socket_pipe const&) = delete;             // Copy construct
    osd_socket_pipe(osd_socket_pipe&&) = delete;                  // Move construct
    osd_socket_pipe& operator=(osd_socket_pipe const&) = delete;  // Copy assign
    osd_socket_pipe& operator=(osd_socket_pipe &&) = delete;      // Move assign
    size_t writeVideoBuffer(uint8_t * buffer, size_t len);
    size_t writeAudioBuffer(uint8_t * buffer, size_t len);
    size_t writeDataBuffer(uint8_t * buffer, size_t len);
    size_t readVideoBuffer(uint8_t * buffer, size_t len);
    size_t readAudioBuffer(uint8_t * buffer, size_t len);
    size_t readDataBuffer(uint8_t * buffer, size_t len);
    bool init(running_machine * machine, const char * socket_path);
    bool isInitialized() { return _isInit; }
    running_machine *machine() const { return m_machine; }
    int screenWidth(int index = 0);
    int screenHeight(int index = 0);
    double screenRefreshRate(int index = 0);
    int screenCount() { return _screenCount; }
    bool audioFlowStarted() { return _audioFlowStarted; }
    void setDataCB(ISocketPipeDataCB * cb) { _datacb = cb; }
protected:
    virtual void dataThreadProc();
    osd_socket_pipe();
    ~osd_socket_pipe();

private:
    void _connectionProc();
    bool _isInit;
    int _serverSocket;
    int _videoStreamSocket;
    int _audioStreamSocket;
    int _dataStreamSocket;
    running_machine *m_machine;                  // pointer to our machine
    std::vector<socketpipe_screen> _screens;
    int _screenCount;
    bool _audioFlowStarted;
    ISocketPipeDataCB * _datacb;
    std::thread * _dataThread;
    sem_t _avStreamReadySem;
};

#endif /* __SOCKETPIPE__ */
