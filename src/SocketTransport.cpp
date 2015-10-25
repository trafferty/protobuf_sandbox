/**************************************************************************
*
*		     Source:  SocketTransport.cpp
*           Project:  ScorpionServer
*
*            Author: trafferty
*              Date: Jan 27, 2015
*
*		Description:
*			> This is the transport interface for sockets.
*
****************************************************************************/

#include <vector>
#include <cstring>
#include <unistd.h>

#include "SocketTransport.h"

SocketTransport::SocketTransport(bool debug) :
   m_Debug(debug),
   m_Name("SocketTransport"),
   m_Invoke_Cnt(0),
   m_CommStarted(false),
   m_ReceiveMode(SOCKET_READ_MODE_BLOCK),
   m_StopOnDisconnect(true),
   m_ReceiveTimeout(1.0),
   m_ReadBlockSize(32768),
   m_ReadBufferSize(m_ReadBlockSize),
   m_CommunicationState(COMM_STATE_NO_CLIENT),
   m_Socket(nullptr),
   m_ConnMode(ISocket::ConnectionMode_t::CONN_MODE_CLIENT),
   m_CmdBuffer(""),
   m_RecvCallbackPtr(0),
   m_CheckDoneCallbackPtr(0)
{
    m_Log = std::shared_ptr<Logger>(new Logger(m_Name, m_Debug));

    m_ReadBuffer = new char[m_ReadBlockSize];
    std::memset(m_ReadBuffer, '\0', m_ReadBlockSize);
}

SocketTransport::~SocketTransport()
{
    delete[] m_ReadBuffer;
}

bool SocketTransport::init()
{
    if (nullptr == m_Socket)
    {
        m_Log->LogError("No Socket connected");
        return false;
    }

    return true;
}

bool SocketTransport::ResetConnection()
{
    bool ret_val;

    ret_val = m_Socket->ResetConnection();
    if (ret_val == false)
    {
        m_Log->LogError("Unable to reset socket connection");
    }

    return ret_val;
}

bool SocketTransport::CloseConnection()
{
    bool ret_val;

    ret_val = m_Socket->CloseConnection();
    if (ret_val == false)
    {
        m_Log->LogError("Unable to reset port connection");
    }

    return ret_val;
}


bool SocketTransport::RegisterRecvCallback(int callbackID, ICallback* callbackPtr)
{
    m_RecvCallbackPtr = callbackPtr;
    m_Log->LogDebug("Registered Receive callback: ", callbackID);

    return true;
}

bool SocketTransport::UseSocket(const std::shared_ptr<ISocket> socket)
{
    m_Socket = socket;
    return true;
}

bool SocketTransport::doWork()
{
    bool ret_val;
    ret_val = update_tx();
    ret_val = update_rx();
    return ret_val;
}

bool SocketTransport::StartComm()
{
    if (nullptr == m_Socket)
    {
        m_Log->LogError("No Socket connected");
        return false;
    }

    if (!m_CommStarted)
    {
        m_ThreadHelper[TX].method = &SocketTransport::update_tx;
        m_ThreadHelper[RX].method = &SocketTransport::update_rx;
        m_ThreadHelper[TX].obj = (this);
        m_ThreadHelper[RX].obj = (this);

        m_ThreadHelper[TX].Done = false;
        m_ThreadHelper[RX].Done = false;
        m_ThreadHelper[TX].SleepTime_us = 1000;
        m_ThreadHelper[RX].SleepTime_us = 1000;

#if 0
        // create the TX Thread
        if (pthread_create(&(m_ThreadHelper[TX].thread_handle), 0,
            SocketTransport::thread_func, (void *)&m_ThreadHelper[TX]) != 0)
        {
            m_Log->LogError("Error spawning update_tx thread");
            return false;
        }
#endif

        // create the RX Thread
        if (pthread_create(&(m_ThreadHelper[RX].thread_handle), 0,
            SocketTransport::thread_func, (void *)&m_ThreadHelper[RX]) != 0)
        {
            m_Log->LogError("Error spawning update_rx thread");
            return false;
        }

        m_CommStarted = true;
    }

    return true;
}

bool SocketTransport::StopComm()
{
    if (m_CommStarted)
    {
        // if receive thread is in listening mode (blocked) we have to first
        // close the connection:
        ISocket::ConnectionState_t connectionState;
        m_Socket->getConnectionState(connectionState);

        if (connectionState == ISocket::STATE_SERVER_PORT_SETUP)
        {
            if (!m_Socket->CloseConnection())
                m_Log->LogError("Error closing socket.");
            else
                m_Log->LogInfo("Socket closed successfully.");

            m_ThreadHelper[RX].Done = true;
            pthread_join(m_ThreadHelper[RX].thread_handle, NULL);
        }
        else
        {
            m_ThreadHelper[RX].Done = true;
            pthread_join(m_ThreadHelper[RX].thread_handle, NULL);

            if (!m_Socket->CloseConnection())
                m_Log->LogError("Error closing comm port.");
            else
                m_Log->LogInfo("Comm port closed successfully.");
        }

        m_CommStarted = false;
    }

    return true;
}

//=============================================================================
// thread_func
//-----------------------------------------------------------------------------
//=============================================================================
void *SocketTransport::thread_func(void *args)
{
    ThreadHelper* threadHelper = static_cast<ThreadHelper*> (args);

    while (!threadHelper->Done)
    {
        (threadHelper->obj->*(threadHelper->method))();
        usleep(threadHelper->SleepTime_us);
    }

    pthread_exit(0);
    return NULL;
}

bool SocketTransport::TransmitData(unsigned char *data_buffer, unsigned int data_size)
{
    bool	ret_val = true;

    if (data_buffer == 0)
    {
        m_Log->LogError("SocketTransport::TransmitData: data_buffer is 0, data_size =", data_size);
        return false;
    }

    // Direct Call
    //rc = m_Socket->writeBlock((unsigned char*)data_buffer, data_size);

    if (ret_val == false)
    {
        m_Log->LogError("SocketTransport::TransmitData - writeBytes failed.");
        return false;
    }

    return true;
}

//=============================================================================
// update_tx_routine
//-----------------------------------------------------------------------------
//=============================================================================
bool SocketTransport::update_tx()
{
    // until we decide to use this thread or not, just sleep a bit...
    usleep(1000*1000);

    return true;
}

//=============================================================================
// update_rx_routine
//-----------------------------------------------------------------------------
//=============================================================================
bool SocketTransport::update_rx()
{
    bool ret_val;
    int numRead = 0;
    std::vector<std::string> cmdStrs;
    std::string client_IP;

    ISocket::ConnectionState_t connectionState;
    ret_val = m_Socket->getConnectionState(connectionState);
    if (ret_val == false)
    {
        m_Log->LogError("Unable to get port state");
        usleep(1000 * 1000);
        return false;
    }

    switch (connectionState)
    {
        case ISocket::STATE_NO_CONNECTION:
            ret_val = m_Socket->ResetConnection();
            if (ret_val == false)
            {
                m_Log->LogError("Unable to reset socket: ", m_Socket->GetName());
                usleep(500 * 1000);
                return false;
            }
            break;

        case ISocket::STATE_SERVER_PORT_SETUP:
            ret_val = m_Socket->ListenForClient(client_IP);
            if (ret_val == false)
            {
                return false;
            }

            m_Socket->getConnectionState(connectionState);
            if (ISocket::ConnectionState_t::STATE_CONNECTED == connectionState)
                m_Log->LogDebug("Client connected from: ", client_IP);
            break;

        case ISocket::STATE_SERVER_LISTENING:
            m_Log->LogDebug("ISocket::STATE_SERVER_LISTENING");
            // not doing anything for this just yet...
            break;

        case ISocket::STATE_CONNECTED:
#if 0        
            // used this just for initial testing...
            ret_val = m_Socket->ListenForTraffic();
            if (ret_val == false)
            {
                m_Log->LogError("Error listening for traffic");
                usleep(500 * 1000);
                return false;
            }
#else
            switch (m_ReceiveMode)
            {
                case SOCKET_READ_MODE_LINE:
                    ret_val = m_Socket->readLine((char*)m_ReadBuffer, m_ReadBlockSize, numRead, m_ReceiveTimeout, m_StopOnDisconnect);
                    break;

                case SOCKET_READ_MODE_BLOCK:
                    ret_val = m_Socket->readBlock(m_ReadBuffer, m_ReadBlockSize-1, numRead, 0);
                    break;

                case SOCKET_READ_MODE_UNTIL:
                   //read_okay = m_Socket->readUntil(m_ReadBuffer, m_ReadBlockSize, numRead, m_CheckDoneCallbackPtr, m_ReceiveTimeout);
                   m_Log->LogError("SOCKET_READ_MODE_UNTIL not yet supported");
                   usleep(5 * 1000);
                   return false;
                   break;
            }
#endif

            if (ret_val == false)
            {
                if (numRead == -1)
                {
                    m_Log->LogWarn("Detected socket closed");
                }

                numRead = 0;

                m_Log->LogDebug("Resetting socket...");
                if ( !m_Socket->ResetConnection() )
                {
                    usleep(500 * 1000);
                    m_Log->LogError("Unable to reset socket connection: ", m_Socket->GetName());
                    return false;
                }
            }

            if (numRead > 0)
            {
               //m_Log->LogDebug("...numRead = ", numRead, ": ", m_ReadBuffer);

                m_CmdBuffer.append(m_ReadBuffer);
                std::memset(m_ReadBuffer, '\0', m_ReadBlockSize);

                std::string::size_type idx = m_CmdBuffer.find('\n');
                while (idx != std::string::npos)
                {
                    cmdStrs.push_back(m_CmdBuffer.substr(0, idx));
                    m_CmdBuffer = m_CmdBuffer.substr(idx + 1);
                    idx = m_CmdBuffer.find('\n');
                }

                for (std::string cmdStr : cmdStrs)
                {
                   if (m_RecvCallbackPtr)
                   {
                      //m_RecvCallbackPtr->Invoke((void *)numRead, (void *)cmdStr.c_str());
                      //m_Log->LogDebug("...Sending to callback: ", cmdStr);
                      m_RecvCallbackPtr->Invoke((void *)++m_Invoke_Cnt, &cmdStr);
                   }
                   else
                   {
                       // no callback so just print to console...
                       m_Log->LogDebug("Rcvd: ", cmdStr);
                   }
                }
            }
            //else
            //    m_Log->LogDebug("No data...");

            break;

        default:
            m_Log->LogError("Socket is in invalid state: ", m_Socket->GetName());
            usleep(500 * 1000);
            return false;
            break;
    }

    return true;
}
