/**************************************************************************
*
*		     Source:   LinuxSocket.cpp
*           Project:  ScorpionServer
*
*            Author: trafferty
*              Date: Jan 27, 2015
*
*		Description:
*		  > class for Win7 TCP/IP implementation of a ISocket 
*
****************************************************************************/

#include <iostream>
#include <sstream>
#include <string>
#include <string.h> // memset
#include <memory>

#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>

#include "LinuxSocket.h"

LinuxSocket::LinuxSocket(const char* name, const bool debug) :
    m_Debug(debug),
    m_Name(name),
	 m_Log(nullptr),
    m_IPAddress(""),
    m_LineMode(false),
    m_ConnectionMode(ISocket::CONN_MODE_SERVER),
    m_ConnectionState(ISocket::STATE_NO_CONNECTION),
    m_ServerListenerPort(0),
    m_ConnectTimeoutSeconds(DEFAULT_CONNECTION_TIMEOUT),
    m_ServerSock(INVALID_SOCKET),
    m_ClientSock(INVALID_SOCKET),
	m_RecvCallbackPtr(nullptr)
{
    m_Log = std::shared_ptr<Logger>(new Logger(m_Name, m_Debug));
}

LinuxSocket::~LinuxSocket()
{
    if (m_ClientSock != INVALID_SOCKET)
    {
        m_Log->LogDebug("[",m_Name,"] Closing open client socket -", m_ClientSock);
        close(m_ClientSock);
        m_ClientSock = INVALID_SOCKET;
    }

    if (m_ServerSock != -1)
    {
        m_Log->LogDebug("[",m_Name,"] Closing open server socket - ", m_ServerSock);
        close(m_ServerSock);
        m_ServerSock = INVALID_SOCKET;
    }

}

bool LinuxSocket::Init(ConnectionMode_t mode, const std::string IPAddress, const int port)
{
    m_ConnectionMode = mode;
    m_IPAddress = IPAddress;
    m_ServerListenerPort = port;

    if (m_ConnectionMode == CONN_MODE_SERVER)
    {
		// Create the socket
		m_ServerSock = socket(AF_INET, SOCK_STREAM, 0);

		if (m_ServerSock == INVALID_SOCKET)
		{
			m_Log->LogError("[",m_Name,"] Error creating socket");
			return false;
		}

		// allow reuse of this socket
		int yes = 1;
		setsockopt(m_ServerSock, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

		m_server_addr.sin_family = AF_INET;
		m_server_addr.sin_port = htons(port);
		m_server_addr.sin_addr.s_addr = htonl(INADDR_ANY);

		m_Log->LogInfo("[",m_Name,"] Setting up server socket on port: %d", m_ServerListenerPort );

		if (SOCKET_ERROR == bind(m_ServerSock, (struct sockaddr*) &m_server_addr, sizeof(m_server_addr)))
		{
			m_Log->LogError("[",m_Name,"] Unable to bind socket");
			return false;
		}

        m_ConnectionState = ISocket::ConnectionState_t::STATE_SERVER_PORT_SETUP;
    }
    else if (m_ConnectionMode == CONN_MODE_CLIENT)
    {
        m_ConnectionState = ISocket::ConnectionState_t::STATE_NO_CONNECTION;
    }

    return true;
}

bool LinuxSocket::ListenForClient(std::string& client_IP)
{
    if ((m_ConnectionMode == CONN_MODE_SERVER) && (m_ServerSock != INVALID_SOCKET))
    {

        // To listen on a socket
        if (listen(m_ServerSock, SOMAXCONN) == SOCKET_ERROR)
        {
            m_Log->LogError("[",m_Name,"] Listen failed...");
            close(m_ServerSock);
            m_ServerSock = INVALID_SOCKET;
            return false;
        }

        struct sockaddr_in client_addr;
        unsigned int addrlen = sizeof(client_addr);

        // Accept a client socket
        m_ClientSock = accept(m_ServerSock, (sockaddr*)&client_addr, &addrlen);

        if (m_ClientSock == INVALID_SOCKET)
        {
            m_Log->LogError("[",m_Name,"] Accept failed...");
            close(m_ServerSock);
            m_ServerSock = INVALID_SOCKET;
            return false;
        }

        fcntl(m_ClientSock, F_SETFL, O_NONBLOCK);

        char connected_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &(m_client_addr.sin_addr), connected_ip, INET_ADDRSTRLEN);
        int port = ntohs(client_addr.sin_port);
        std::stringstream ss;
        ss << connected_ip << ":" << port;
        client_IP = ss.str();

        m_ConnectionState = ISocket::ConnectionState_t::STATE_CONNECTED;
    }
    else
    {
        m_Log->LogError("[",m_Name,"] Listen failed: ConnMode != server, or server socket invalid");
        return false;
    }

    return true;
}

bool LinuxSocket::ListenForTraffic()
{
//    char recvbuf[500];
//    WSADATA wsaData;
//    int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
//
//    // reveice until the client shutdown the connection
//    do {
//        result = recv(m_ClientSock, recvbuf, 500, 0);
//
//        m_Log->LogDebug("Recv returned : ", result);
//
//        if (result > 0)
//        {
//            char msg[500];
//            memset(&msg, 0, sizeof(msg));
//            strncpy_s(msg, recvbuf, result);
//
//            if (m_RecvCallbackPtr)
//            {
//                m_RecvCallbackPtr->Invoke((void *)result, (void *)msg);
//                //LOG_INFO("(%s) Rcvd:: %s", Name, m_ReadBuffer);
//            }
//            else
//            {
//                // no callback so just print to console...
//                m_Log->LogDebug("Received: ", msg);
//            }
//
//            if (_stricmp(msg, "quit") == 0)
//            {
//                m_Log->LogDebug("recieved quit cmd");
//                break;
//            }
//        }
//        else if (result == 0)
//            m_Log->LogWarn("Connection closed");
//        else{
//            m_Log->LogError("Recv failed: ", WSAGetLastError());
//            closesocket(m_ClientSock);
//            WSACleanup();
//        }
//    } while (result > 0);

    return true;
}

bool LinuxSocket::sendData(const char* data, int numBytes)
{
    if (numBytes == 0)
        return true;

    if (m_ClientSock == INVALID_SOCKET)
    {
        m_Log->LogError("[",m_Name,"] No connected client socket");
        return false;
    }

    int iSendResult = send(m_ClientSock, data, numBytes, 0);

    if (iSendResult == SOCKET_ERROR)
    {
    	m_Log->LogError("[",m_Name,"] Send failed...");
        return false;
    }

    //m_Log->LogDebug("Bytes sent: ", iSendResult);

    return true;
}

bool LinuxSocket::readLine(char* rcvBuffer, int buffer_length, int& numRead, double timeout_s, bool stopOnDisconnect)
{
	int     timeout_us;
    int     result;
    int     socketError;
    bool    firstChar = false;
    bool    done = false;

    timeout_us = timeout_s * 1e6;

    memset(rcvBuffer, '\0', buffer_length);
    numRead = 0;

    while (true)
    {
        result = read(m_ClientSock, &rcvBuffer[numRead], 1);
        switch (result)
        {
            case -1:
				switch (errno)
				{
				 case EINTR:
					 break;

				 case EAGAIN:
				 //case EWOULDBLOCK:
					 usleep(250);
					 timeout_us -= 250;
					 break;

				 default:
	                 m_Log->LogError("[",m_Name,"] socket read failed: ", strerror(errno));
					 return false;
				}
				break;

			case 0:
                if (stopOnDisconnect)
                {
                    numRead = -1;
                    return false;
                }
                else
                {
            		usleep(250);
            		timeout_us -= 250;
                    break;
                }
            case 1:
                if (!firstChar)
                {
                    firstChar = true;
                    timeout_us = timeout_s * 1e6;
                }

                switch (rcvBuffer[numRead])
                {
                    case '\n':
                    case '\r':
                        rcvBuffer[numRead] = '\0';
                        done = true;
                        break;
                    default:
                        numRead++;
                        if (numRead >= buffer_length)	// should never be greater since we read 1 byte at a time
                            done = true;
                        break;
                }
                break;
            default:
                m_Log->LogDebug("[",m_Name,"] Too many bytes read: ", numRead);
                return false;
        }

        if (done)
            break;

        if (timeout_us <= 0)
        {
            if (numRead > 0)
            {
                m_Log->LogDebug("[",m_Name,"] Timeout in the middle of readLine.  Bytes read before timeout: ", numRead);
                return false;
            }
            else
            {
                m_Log->LogDebug("[",m_Name,"] Timeout in readLine.  Bytes read before timeout: ", numRead);
                return true;
            }
        }
    }

    if (!done)
    {
        m_Log->LogError("[",m_Name,"] Buffer full before end of line found");
        return false;
    }

    return true;
}
#include <cstring>

bool LinuxSocket::readBlock(char* rcvBuffer, int buffer_length, int& numRead, double timeout_s)
{
    bool retVal = true;

    numRead = 0;

    do
    {
        if (m_ClientSock != SOCKET_ERROR)
        {
            int inBytes = recv(m_ClientSock, rcvBuffer, buffer_length, 0);

            if (inBytes == 0) 
            {
                // set to -1 to indicate EOF or client drop off
                numRead = -1;
                retVal = false;
                break;
            }
            else if (inBytes < 0) 
            {
                //~ int socketError = WSAGetLastError();
                //~ if (WSAEWOULDBLOCK != socketError)
                //~ {
                    //~ m_Log->LogError("recv failed: ", WSAGetLastError());
                    //~ retVal = false;
                //~ }
                //~ else
                {
					m_Log->LogError("recv failed: ", inBytes);
                    numRead = 0;
                }
                break;
            }
            numRead = inBytes;
        }
    } while (false);

    return retVal;
}


bool LinuxSocket::RegisterRecvCallback(int callbackID, ICallback* callbackPtr)
{
    m_RecvCallbackPtr = callbackPtr;

    m_Log->LogDebug("Registered Receive callback: ", callbackID);
    return true;
}

bool LinuxSocket::ResetConnection()
{ 
    if (m_ConnectionMode == ISocket::CONN_MODE_CLIENT)
    {
        m_Log->LogDebug("Socket port set to CONN_MODE_CLIENT, reconnecting...");
        /*
        * try to connect to the server
        */

        m_Log->LogDebug("CONN_MODE_CLIENT Socket type not yet implemented");

        //if (!reconnect(c))
        //{
        //    m_ConnectionState = STATE_NO_CONNECTION;
        //    return false;
        //}
        //else
        //{
        //    m_ConnectionState = STATE_CONNECTED;
        //}
    }
    else if (m_ConnectionMode == ISocket::CONN_MODE_SERVER)
    {
        m_Log->LogDebug("Socket port set to CONN_MODE_SERVER");
        if (m_ServerListenerPort <= 0)
        {
            m_Log->LogError("ISocket::NO_LISTENER_PORT_SPECIFIED");
            return false;
        }
        m_ConnectionState = STATE_SERVER_PORT_SETUP;
    }

    return true; 

}
bool LinuxSocket::CloseConnection()
{
    if (m_ClientSock != INVALID_SOCKET)
    {
        m_Log->LogDebug("Closing open client socket -", m_ClientSock);
        closesocket(m_ClientSock);
        m_ClientSock = INVALID_SOCKET;
    }

    if (m_ServerSock != INVALID_SOCKET)
    {
        m_Log->LogDebug("Closing open server socket - ", m_ServerSock);
        closesocket(m_ServerSock);
        m_ServerSock = INVALID_SOCKET;
    }

    return true; 
}

bool LinuxSocket::getConnectionState(ISocket::ConnectionState_t &state)
{
    state = m_ConnectionState;
    return true;
}
