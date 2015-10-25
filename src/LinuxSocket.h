/**************************************************************************
*
*		     Source:   LinuxSocket.h
*		    Project:  imgAPO
*
*		     Author: trafferty
*		       Date: Jun, 6, 2015
*
*		Description:
*		  > class for TCP/IP implementation under linux.
*
****************************************************************************/

#ifndef  LinuxSocket_H
#define  LinuxSocket_H

#include <iostream>
#include <sstream>
#include <string>
#include <memory>

//#include <unistd.h>
//#include <sys/types.h>
//#include <sys/socket.h>
#include <netinet/in.h>


#include "Logger.h"
#include "ISocket.h"
#include "Callback.h"

#define MAXCONNECTIONS	1
#define INVALID_SOCKET -1
#define SOCKET_ERROR -1

// Default connection timeout is 3 seconds
#define DEFAULT_CONNECTION_TIMEOUT   3

class  LinuxSocket : public ISocket
{
  public:
             LinuxSocket(const char* name, const bool debug = false);
    virtual ~LinuxSocket();

    bool init(ConnectionMode_t mode, const std::string IPAddress, const int port); 

    bool RegisterRecvCallback(int callbackID, ICallback* callbackPtr);

    bool readLine(char* rcvBuffer, int buffer_length, int& numRead, double timeout_s = 1.0, bool stopOnDisconnect = false);
    bool readBlock(char* rcvBuffer, int buffer_length, int& numRead, double timeout_s = 1.0);
    ////bool readUntil(unsigned char* bytes, unsigned int numRequested, int& numread, ICallback* RecvCallbackPtr, double timeoutSecs);
    //bool readUntilToken(char* bytes, unsigned int numRequested, int& numread, const char* tokStr, double timeoutSecs);

    bool sendData(const char* data, int numBytes);

    bool ListenForClient(std::string& client_IP);

    bool ListenForTraffic();

    bool ResetConnection ();
    bool CloseConnection ();

    bool getConnectionState(ISocket::ConnectionState_t &state );

    std::string GetName();

  protected:
    bool m_Debug;
    std::string m_Name;
    std::shared_ptr<Logger> m_Log;

    std::string m_IPAddress;
    int m_Port;
    bool m_LineMode;

    ISocket::ConnectionMode_t m_ConnectionMode;
    ISocket::ConnectionState_t m_ConnectionState;

    int m_ConnectTimeoutSeconds;

    int m_ServerSock;
    int m_ClientSock;

    sockaddr_in m_server_addr; // Socket Struct
    sockaddr_in m_client_addr; // Socket Struct

    struct timeval m_readTimeout;

    ICallback* m_RecvCallbackPtr;

    bool reconnect();
};

#endif
