// **********************************************************************
//
// Copyright (c) 2003-2015 ZeroC, Inc. All rights reserved.
//
// This copy of Ice is licensed to you under the terms described in the
// ICE_LICENSE file included in this distribution.
//
// **********************************************************************

#include <IceBT/StreamSocket.h>
#include <IceBT/EndpointInfo.h>
#include <IceBT/Instance.h>
#include <IceBT/Util.h>

#include <Ice/LoggerUtil.h>
#include <Ice/Properties.h>

using namespace std;
using namespace Ice;
using namespace IceBT;

IceBT::StreamSocket::StreamSocket(const InstancePtr& instance, const SocketAddress& addr) :
    IceInternal::NativeInfo(createSocket()),
    _instance(instance),
    _addr(addr),
    _state(StateNeedConnect)
{
    init();
    if(doConnect(_fd, _addr))
    {
        _state = StateConnected;
    }
    _desc = fdToString(_fd);
}

IceBT::StreamSocket::StreamSocket(const InstancePtr& instance, SOCKET fd) :
    IceInternal::NativeInfo(fd),
    _instance(instance),
    _state(StateConnected)
{
    init();
    _desc = fdToString(fd);
}

IceBT::StreamSocket::~StreamSocket()
{
    assert(_fd == INVALID_SOCKET);
}

IceInternal::SocketOperation
IceBT::StreamSocket::connect(IceInternal::Buffer& readBuffer, IceInternal::Buffer& writeBuffer)
{
    if(_state == StateNeedConnect)
    {
        _state = StateConnectPending;
        return IceInternal::SocketOperationConnect;
    }
    else if(_state <= StateConnectPending)
    {
        doFinishConnect(_fd);
        _desc = fdToString(_fd);
        _state = StateConnected;
    }

    assert(_state == StateConnected);
    return IceInternal::SocketOperationNone;
}

bool
IceBT::StreamSocket::isConnected()
{
    return _state == StateConnected;
}

size_t
IceBT::StreamSocket::getSendPacketSize(size_t length)
{
    return length;
}

size_t
IceBT::StreamSocket::getRecvPacketSize(size_t length)
{
    return length;
}

void
IceBT::StreamSocket::setBufferSize(int rcvSize, int sndSize)
{
    assert(_fd != INVALID_SOCKET);

    if(rcvSize > 0)
    {
        //
        // Try to set the buffer size. The kernel will silently adjust
        // the size to an acceptable value. Then read the size back to
        // get the size that was actually set.
        //
        IceInternal::setRecvBufferSize(_fd, rcvSize);
        int size = IceInternal::getRecvBufferSize(_fd);
        if(size > 0 && size < rcvSize)
        {
            //
            // Warn if the size that was set is less than the requested size and
            // we have not already warned.
            //
            IceInternal::BufSizeWarnInfo winfo = _instance->getBufSizeWarn(EndpointType);
            if(!winfo.rcvWarn || rcvSize != winfo.rcvSize)
            {
                Ice::Warning out(_instance->logger());
                out << "BT receive buffer size: requested size of " << rcvSize << " adjusted to " << size;
                _instance->setRcvBufSizeWarn(EndpointType, rcvSize);
            }
        }
    }

    if(sndSize > 0)
    {
        //
        // Try to set the buffer size. The kernel will silently adjust
        // the size to an acceptable value. Then read the size back to
        // get the size that was actually set.
        //
        IceInternal::setSendBufferSize(_fd, sndSize);
        int size = IceInternal::getSendBufferSize(_fd);
        if(size > 0 && size < sndSize)
        {
            // Warn if the size that was set is less than the requested size and
            // we have not already warned.
            IceInternal::BufSizeWarnInfo winfo = _instance->getBufSizeWarn(EndpointType);
            if(!winfo.sndWarn || sndSize != winfo.sndSize)
            {
                Ice::Warning out(_instance->logger());
                out << "BT send buffer size: requested size of " << sndSize << " adjusted to " << size;
                _instance->setSndBufSizeWarn(EndpointType, sndSize);
            }
        }
    }
}

IceInternal::SocketOperation
IceBT::StreamSocket::read(IceInternal::Buffer& buf)
{
    buf.i += read(reinterpret_cast<char*>(&*buf.i), buf.b.end() - buf.i);
    return buf.i != buf.b.end() ? IceInternal::SocketOperationRead : IceInternal::SocketOperationNone;
}

IceInternal::SocketOperation
IceBT::StreamSocket::write(IceInternal::Buffer& buf)
{
    buf.i += write(reinterpret_cast<const char*>(&*buf.i), buf.b.end() - buf.i);
    return buf.i != buf.b.end() ? IceInternal::SocketOperationWrite : IceInternal::SocketOperationNone;
}

ssize_t
IceBT::StreamSocket::read(char* buf, size_t length)
{
    assert(_fd != INVALID_SOCKET);

    size_t packetSize = length;
    ssize_t read = 0;

    while(length > 0)
    {
        ssize_t ret = ::recv(_fd, buf, packetSize, 0);
        if(ret == 0)
        {
            Ice::ConnectionLostException ex(__FILE__, __LINE__);
            ex.error = 0;
            throw ex;
        }
        else if(ret == SOCKET_ERROR)
        {
            if(IceInternal::interrupted())
            {
                continue;
            }

            if(IceInternal::noBuffers() && packetSize > 1024)
            {
                packetSize /= 2;
                continue;
            }

            if(IceInternal::wouldBlock())
            {
                return read;
            }

            if(IceInternal::connectionLost())
            {
                Ice::ConnectionLostException ex(__FILE__, __LINE__);
                ex.error = IceInternal::getSocketErrno();
                throw ex;
            }
            else
            {
                Ice::SocketException ex(__FILE__, __LINE__);
                ex.error = IceInternal::getSocketErrno();
                throw ex;
            }
        }

        buf += ret;
        read += ret;
        length -= ret;

        if(packetSize > length)
        {
            packetSize = length;
        }
    }
    return read;
}

ssize_t
IceBT::StreamSocket::write(const char* buf, size_t length)
{
    assert(_fd != INVALID_SOCKET);

    size_t packetSize = length;

    ssize_t sent = 0;
    while(length > 0)
    {
        ssize_t ret = ::send(_fd, buf, packetSize, 0);
        if(ret == 0)
        {
            Ice::ConnectionLostException ex(__FILE__, __LINE__);
            ex.error = 0;
            throw ex;
        }
        else if(ret == SOCKET_ERROR)
        {
            if(IceInternal::interrupted())
            {
                continue;
            }

            if(IceInternal::noBuffers() && packetSize > 1024)
            {
                packetSize /= 2;
                continue;
            }

            if(IceInternal::wouldBlock())
            {
                return sent;
            }

            if(IceInternal::connectionLost())
            {
                Ice::ConnectionLostException ex(__FILE__, __LINE__);
                ex.error = IceInternal::getSocketErrno();
                throw ex;
            }
            else
            {
                Ice::SocketException ex(__FILE__, __LINE__);
                ex.error = IceInternal::getSocketErrno();
                throw ex;
            }
        }

        buf += ret;
        sent += ret;
        length -= ret;

        if(packetSize > length)
        {
            packetSize = length;
        }
    }
    return sent;
}

void
IceBT::StreamSocket::close()
{
    assert(_fd != INVALID_SOCKET);
    try
    {
        IceInternal::closeSocket(_fd);
        _fd = INVALID_SOCKET;
    }
    catch(const Ice::SocketException&)
    {
        _fd = INVALID_SOCKET;
        throw;
    }
}

const string&
IceBT::StreamSocket::toString() const
{
    return _desc;
}

void
IceBT::StreamSocket::init()
{
    IceInternal::setBlock(_fd, false);

    Int rcvSize = _instance->properties()->getPropertyAsInt("IceBT.RcvSize");
    Int sndSize = _instance->properties()->getPropertyAsInt("IceBT.SndSize");

    setBufferSize(rcvSize, sndSize);
}
