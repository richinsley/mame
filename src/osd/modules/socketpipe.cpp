// jl
#include "socketpipe.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <thread>

struct sockaddr_un server;

osd_socket_pipe& osd_socket_pipe::Instance()
{
    // thread-safe in C++11.
    static osd_socket_pipe myInstance;

    // Return a reference to our instance.
    return myInstance;
}

osd_socket_pipe::osd_socket_pipe() :
    _isInit(false),
    _serverSocket(-1),
    _videoStreamSocket(-1),
    _audioStreamSocket(-1),
    _dataStreamSocket(-1),
    m_machine(NULL),
    _screenCount(-1),
    _audioFlowStarted(false),
    _datacb(NULL),
    _dataThread(NULL)
{

}

osd_socket_pipe::~osd_socket_pipe()
{

}

void osd_socket_pipe::_connectionProc()
{
    bool sentVidAudComplete = false;
    while(_videoStreamSocket == -1 || _audioStreamSocket == -1 || _dataStreamSocket == -1)
    {
        if(!sentVidAudComplete && _videoStreamSocket != -1 && _audioStreamSocket != -1)
        {
            sem_post(&_avStreamReadySem);
            sentVidAudComplete = true;
        }

        int nsock = ::accept(_serverSocket, 0, 0);
        if(nsock == -1)
        {
            perror("accept");
            break;
        }

        // read 4 byte stream identifier
        char id_buf[4];
        char ok_resp = 0;
        char fail_resp = 1;
        int r = ::read(nsock, id_buf, 4);
        if(r != 4)
        {
            perror("invalid header");
            break;
        }

        if(id_buf[0] == 'V' && id_buf[1] == 'D' && id_buf[2] == 'E' && id_buf[3] == 'O')
        {
            _videoStreamSocket = nsock;
            ::write(nsock, &ok_resp, sizeof(ok_resp));
            continue;
        }
        else if(id_buf[0] == 'A' && id_buf[1] == 'U' && id_buf[2] == 'D' && id_buf[3] == 'O')
        {
            _audioStreamSocket = nsock;
            ::write(nsock, &ok_resp, sizeof(ok_resp));
            continue;
        }
        else if(id_buf[0] == 'D' && id_buf[1] == 'A' && id_buf[2] == 'T' && id_buf[3] == 'A')
        {
            _dataStreamSocket = nsock;
            ::write(nsock, &ok_resp, sizeof(ok_resp));
            _dataThread = new std::thread(&osd_socket_pipe::dataThreadProc, this);
            continue;
        }
        else
        {
            ::write(nsock, &ok_resp, sizeof(fail_resp));
            continue;
        }
    }
}

bool osd_socket_pipe::init(running_machine *machine, const char * socket_path)
{
   // get the machine screen infos
   _screenCount = 0;
   _screens.clear();
   m_machine = machine;
   for (const screen_device &screendev : screen_device_enumerator(machine->root_device()))
	{
       socketpipe_screen s;
       if (screendev.screen_type() != SCREEN_TYPE_VECTOR)
		{
           s.width = screendev.width();
           s.height = screendev.height();
           s.refresh_rate = ATTOSECONDS_TO_HZ(screendev.refresh_attoseconds());
           s.pixclock = s.width * s.height * s.refresh_rate;
           s.screen_type = screendev.screen_type();
       } else {
           s.width = 640;
           s.height = 480;
           s.refresh_rate = 0;
           s.pixclock = s.width * s.height * s.refresh_rate;
           s.screen_type = SCREEN_TYPE_VECTOR;
       }
       _screens.push_back(s);
       _screenCount++;
   }

    // wait on unix socket connection
    unlink(socket_path);
    _serverSocket = socket(AF_UNIX, SOCK_STREAM, 0);
    if (_serverSocket < 0) {
        perror("opening stream socket");
        return false;
    }
    server.sun_family = AF_UNIX;
    strcpy(server.sun_path, socket_path);
    if (bind(_serverSocket, (struct sockaddr *) &server, sizeof(struct sockaddr_un)))
    {
        perror("binding stream socket");
        return false;
    }
    listen(_serverSocket, 5);

    // our server connection thread will signal back on _avStreamReadySem when the audio and video
    // channels are opened, but will continue until the data channel is opened.
    sem_init(&_avStreamReadySem, 0, 0);
    std::thread conProc(&osd_socket_pipe::_connectionProc, this);
    conProc.detach();
    sem_wait(&_avStreamReadySem);
    sem_destroy(&_avStreamReadySem);

    _isInit = true;

    return true;
}

size_t osd_socket_pipe::writeVideoBuffer(uint8_t * buffer, size_t len)
{
    return ::write(_videoStreamSocket, buffer, len);
}

size_t osd_socket_pipe::writeAudioBuffer(uint8_t * buffer, size_t len)
{
    if(!_audioFlowStarted)
        _audioFlowStarted = true;
    return ::write(_audioStreamSocket, buffer, len);
}

size_t osd_socket_pipe::writeDataBuffer(uint8_t * buffer, size_t len)
{
    return ::write(_dataStreamSocket, buffer, len);
}

size_t osd_socket_pipe::readVideoBuffer(uint8_t * buffer, size_t len)
{
    return ::read(_videoStreamSocket, (void*)buffer, len);
}

size_t osd_socket_pipe::readAudioBuffer(uint8_t * buffer, size_t len)
{
    return ::read(_audioStreamSocket, (void*)buffer, len);
}

size_t osd_socket_pipe::readDataBuffer(uint8_t * buffer, size_t len)
{
    return ::read(_dataStreamSocket, (void*)buffer, len);
}

int osd_socket_pipe::screenWidth(int index)
{
    if(index + 1 > _screenCount || index < 0)
        return -1;

    return _screens[index].width;
}

int osd_socket_pipe::screenHeight(int index)
{
    if(index + 1 > _screenCount || index < 0)
        return -1;

    return _screens[index].height;
}

double osd_socket_pipe::screenRefreshRate(int index)
{
    if(index + 1 > _screenCount || index < 0)
        return -1;

    return _screens[index].refresh_rate;
}

void osd_socket_pipe::dataThreadProc()
{
    // yep, totally unsafe right now.. ehem.. TODO
    uint8_t sbuf[4];
    uint8_t data[1024];
    while(1)
    {
        size_t r = readDataBuffer(sbuf, 4);
        if(r == 4)
        {
            uint32_t msize = *(uint32_t*)sbuf;
            readDataBuffer(data, (size_t)msize);
            if(_datacb)
            {
                _datacb->onData(data, msize);
            }
        }
    }
}