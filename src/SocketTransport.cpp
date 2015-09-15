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
#include "stdafx.h"

#include "SocketTransport.h"

SocketTransport::SocketTransport(bool debug) :
m_Name("SocketTransport"),
m_CommunicationState(COMM_STATE_NO_CLIENT),
m_RecvCallbackPtr(0),
m_CheckDoneCallbackPtr(0),
m_Socket(NULL),
m_ReceiveMode(SOCKET_READ_MODE_BLOCK),
m_StopOnDisconnect(true),
m_ReceiveTimeout(1.0),
m_ReadBlockSize(32768),
m_CommStarted(false),
m_Debug(debug)
{
    m_Log = std::shared_ptr<Logger>(new Logger(m_Name, m_Debug));

    m_ReadBuffer = new char[m_ReadBlockSize];
    memset(m_ReadBuffer, '\0', m_ReadBlockSize);
    m_ReadBufferSize = m_ReadBlockSize;
}

SocketTransport::~SocketTransport()
{
    delete[] m_ReadBuffer;
}

bool SocketTransport::Init()
{
    if (NULL == m_Socket)
    {
        m_Log->LogError("No Socket connected");
        return false;
    }

    //m_Socket->getConnectionMode(m_ConnMode);

    //switch (m_ConnMode)
    //{
    //   case CommPort::CONN_MODE_CLIENT:
    //   {
    //      LOG_INFO("(%s) Transport setup for client CommPort", m_Name.c_str());
    //      break;
    //   }
    //   case CommPort::CONN_MODE_SERVER:
    //   {
    //      LOG_INFO("(%s) Transport setup for server CommPort", m_Name.c_str());
    //      break;
    //   }
    //   case CommPort::CONN_MODE_SERIAL:
    //   {
    //      LOG_INFO("(%s) Transport setup for serial CommPort", m_Name.c_str());
    //      break;
    //   }
    //}

    return true;
}


//ISocket::ConnectionState_t SocketTransport::GetCommPortState()
//{
//   bool rc;
//   ISocket::ConnectionState_t connectionState;
//
//   rc = m_Socket->getConnectionState(connectionState);
//   if (rc == false)
//   {
//      printf("(%s) Unable to get port state", m_Name.c_str());
//      connectionState = ISocket::STATE_UNKNOWN;
//      return connectionState;
//   }
//
//   return connectionState;
//}

//bool SocketTransport::GetCommPortName(char* m_Name, )
//{
//   bool rc;
//
//   rc = m_CommPort->getPortName(m_Name, c);
//   if (rc == false)
//      LOG_ERROR("(%s) Unable to get port m_Name", m_Name.c_str());
//
//   return rc;
//}

bool SocketTransport::ResetConnection()
{
    bool rc;

    rc = m_Socket->ResetConnection();
    if (rc == false)
    {
        m_Log->LogError("Unable to reset socket connection");
    }

    return rc;
}

bool SocketTransport::CloseConnection()
{
    bool rc;

    rc = m_Socket->CloseConnection();
    if (rc == false)
    {
        m_Log->LogError("Unable to reset port connection");
    }

    return rc;
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

bool SocketTransport::Update()
{
    //bool rc;
    //   rc = update_tx(c);
    //   rc = update_rx(c);
    return true;
}

bool SocketTransport::StartComm()
{
    if (NULL == m_Socket)
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
        m_ThreadHelper[TX].SleepTime_ms = 1;
        m_ThreadHelper[RX].SleepTime_ms = 1;

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
#if 0
        m_ThreadHelper[TX].Done = true;
        pthread_join(m_ThreadHelper[TX].thread_handle, NULL);
#endif

        // if receive thread is in listening mode (blocked) we have to first
        // close the connection:
        ISocket::ConnectionState_t connectionState;
        m_Socket->getConnectionState(connectionState);

        if (connectionState == ISocket::STATE_SERVER_PORT_SETUP)
        {
            if (!m_Socket->CloseConnection())
                m_Log->LogError("Error closing socket.");
            else
                m_Log->LogError("Socket closed successfully.");

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
                m_Log->LogError("Comm port closed successfully.");
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
        Sleep(threadHelper->SleepTime_ms);
    }

    pthread_exit(0);
    return NULL;
}

//bool SocketTransport::SetReceiveOptions(receiveMode_t mode, bool stopOnDisconnect, unsigned int blockSize, float timeoutSecs, )
//{
//	if (m_CommStarted)
//	{
//		printf("(%s) Cannot change receive options after comm thread has started", m_Name.c_str());
//		return false;
//	}
//
//	if (mode == RECEIVE_MODE_READ_UNTIL)
//	{
//		if (!m_CheckDoneCallbackPtr)
//		{
//			printf("(%s) No CheckDoneCallback registered", m_Name.c_str());
//			return false;
//		}
//	}
//
//	m_StopOnDisconnect = stopOnDisconnect;
//	m_ReceiveTimeout = timeoutSecs;
//	m_ReceiveMode = mode;
//
//	if (blockSize > m_ReadBufferSize)
//	{
//		delete[] m_ReadBuffer;
//		m_ReadBuffer = new unsigned char[blockSize];
//		m_ReadBufferSize = blockSize;
//	}
//	m_ReadBlockSize = blockSize;
//
//	return true;
//}

bool SocketTransport::TransmitData(unsigned char *data_buffer, unsigned int data_size)
{
    bool	rc = true;

    if (data_buffer == 0)
    {
        m_Log->LogError("SocketTransport::TransmitData: data_buffer is 0");
        return false;
    }

    // Direct Call
    //rc = m_Socket->writeBlock((unsigned char*)data_buffer, data_size);

    if (rc == false)
    {
        m_Log->LogError("SocketTransport::TransmitData - m_CommPort->writeBytes failed.");
        return false;
    }

    return true;
}

//bool SocketTransport::ReceiveData(unsigned char *data_buffer, unsigned int *data_size)
//{
//   unsigned int bytes_read = 0;
//
//   if (data_buffer == 0)
//   {
//      return false;
//   }
//
//   *data_size = bytes_read;
//
//   LOG_ERROR("(%s) Is not implemented", m_Name.c_str());
//
//   return true;
//}


//char *SocketTransport::GetLastErrorString(void)
//{
//   return m_LastErrorString;
//}

//=============================================================================
// update_tx_routine
//-----------------------------------------------------------------------------
//=============================================================================
bool SocketTransport::update_tx()
{
    // until we decide to use this thread or not, just sleep a bit...
    Sleep(1000);

    return true;
}

#if CONN_MODE_CLIENT == 0
char*	connModeString[] =
{
    "CONN_MODE_CLIENT",
    "CONN_MODE_SERVER",
    "CONN_MODE_SERIAL"
};
#endif

//=============================================================================
// update_rx_routine
//-----------------------------------------------------------------------------
//=============================================================================
bool SocketTransport::update_rx()
{
    bool rc;
    int numRead = 0;
    std::vector<std::string> cmdStrs;
    std::string client_IP;

    ISocket::ConnectionState_t connectionState;
    rc = m_Socket->getConnectionState(connectionState);
    if (rc == false)
    {
        m_Log->LogError("Unable to get port state");
        Sleep(1 * 1000);
        return false;
    }

    switch (connectionState)
    {
        case ISocket::STATE_NO_CONNECTION:
            rc = m_Socket->ResetConnection();
            if (rc == false)
            {
                m_Log->LogError("Unable to reset port: ", connModeString[m_ConnMode]);
                Sleep(500);
                return false;
            }
            break;

        case ISocket::STATE_SERVER_PORT_SETUP:
            m_Log->LogDebug("Listening for client...");
            rc = m_Socket->ListenForClient(client_IP);
            if (rc == false)
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
            rc = m_Socket->ListenForTraffic();
            if (rc == false)
            {
                m_Log->LogError("Error listening for traffic");
                Sleep(500);
                return false;
            }
#else

            switch (m_ReceiveMode)
            {
                case SOCKET_READ_MODE_LINE:
                    rc = m_Socket->readLine((char*)m_ReadBuffer, m_ReadBlockSize, numRead, m_ReceiveTimeout, m_StopOnDisconnect);
                    break;

                case SOCKET_READ_MODE_BLOCK:
#if 0
                    char *recvBuffer = m_ReadBuffer;

                    rc = m_Socket->readBlock(&recvBuffer[std::strlen(recvBuffer)], 0, numRead, 0);

                    if (numRead > 0)
                    {
                        char *result = recvBuffer;

                        while ((result = std::strchr(result, '\n')) != NULL)
                        {
                            int idx = (int)(result - recvBuffer);
                            char *tmp = (char *)calloc(idx + 1, sizeof(char));

                            cmdStrs.push_back(std::strncpy(tmp, recvBuffer, idx));
                            //cmdStrs.push_back(m_RecvBuffer.substr(0, idx));

                            // Increment result, otherwise we'll find target at the same location
                            ++result;
                            recvBuffer = result;
                        }

                        if (std::strlen(recvBuffer) > 0)
                        {
                            char *tmp = (char *)calloc(std::strlen(recvBuffer) + 1, sizeof(char));
                            std::strncpy(tmp, recvBuffer, std::strlen(recvBuffer));
                            std::memset(m_ReadBuffer, '\0', m_ReadBlockSize);
                            std::strncpy(m_ReadBuffer, tmp, std::strlen(tmp));
                        }
                        else
                        {
                            std::memset(m_ReadBuffer, '\0', m_ReadBlockSize);
                        }
                    }
#else
                    rc = m_Socket->readBlock(m_ReadBuffer, m_ReadBlockSize-1, numRead, 0);
#endif
                    break;

                    //case RECEIVE_MODE_READ_UNTIL:
                        //rc = m_Socket->readUntil(m_ReadBuffer, m_ReadBlockSize, numRead, m_CheckDoneCallbackPtr, m_ReceiveTimeout);
                        //break;
            }
#endif

            if (rc == false)
            {
                if (numRead == -1)
                {
                    m_Log->LogWarn("Detected socket closed");
                }

                numRead = 0;

                m_Log->LogDebug("Resetting socket...");
                rc = m_Socket->ResetConnection();
                if (rc == false)
                {
                    Sleep(500);
                    m_Log->LogError("Unable to reset socket connection: ", connModeString[m_ConnMode]);
                    return false;
                }
            }

            if (numRead > 0)
            {
                m_CmdBuffer.append(m_ReadBuffer);
                std::memset(m_ReadBuffer, '\0', m_ReadBlockSize);

                std::string::size_type idx = m_CmdBuffer.find('\n');
                while (idx != std::string::npos)
                {
                    cmdStrs.push_back(m_CmdBuffer.substr(0, idx));
                    m_CmdBuffer = m_CmdBuffer.substr(idx + 1);
                    idx = m_CmdBuffer.find('\n');
                }

                if (m_RecvCallbackPtr)
                {
                    for (std::string cmdStr : cmdStrs)
                    { 
                        //m_RecvCallbackPtr->Invoke((void *)numRead, (void *)cmdStr.c_str());
                        m_RecvCallbackPtr->Invoke((void *)++m_Invoke_Cnt, &cmdStr);
                    }
                }
                else
                {
                    // no callback so just print to console...
                    m_Log->LogDebug("Rcvd: ", m_ReadBuffer);
                }
            }
            //else
            //    m_Log->LogDebug("No data...");

            break;

        default:
            m_Log->LogError("Socket is in invalid state: ", connModeString[m_ConnMode]);
            Sleep(500);
            return false;
            break;
    }

    return true;
}

