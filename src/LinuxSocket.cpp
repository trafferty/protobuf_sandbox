/**************************************************************************
 *
 *          Source:   LinuxSocket.cpp
 *           Project:  ScorpionServer
 *
 *            Author: trafferty
 *              Date: Jan 27, 2015
 *
 *     Description:
 *       > class for Linux TCP/IP implementation of an ISocket
 *
 ****************************************************************************/

#include <iostream>
#include <sstream>
#include <string>
#include <string.h>

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
m_Port(0),
m_LineMode(false),
m_ConnectionMode(ISocket::CONN_MODE_SERVER),
m_ConnectionState(ISocket::STATE_NO_CONNECTION),
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

bool LinuxSocket::init(ConnectionMode_t mode, const std::string IPAddress, const int port)
{
   m_ConnectionMode = mode;
   m_IPAddress = IPAddress;
   m_Port = port;

   if (m_ConnectionMode == CONN_MODE_SERVER)
   {
      // Create the listening socket
      m_ServerSock = socket(AF_INET, SOCK_STREAM, 0);

      if (m_ServerSock == INVALID_SOCKET)
      {
         m_Log->LogError("[",m_Name,"] Error creating listening socket");
         return false;
      }

      // set to non-blocking mode...
      int status = fcntl(m_ServerSock, F_SETFL, fcntl(m_ServerSock, F_GETFL, 0) | O_NONBLOCK);
      if (status == -1)
      {
         m_Log->LogError("[",m_Name,"] Could not create non-blocking socket...");
         perror("calling fcntl");
      }

      // allow reuse of this socket
      int yes = 1;
      setsockopt(m_ServerSock, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

      m_server_addr.sin_family = AF_INET;
      m_server_addr.sin_port = htons(port);
      m_server_addr.sin_addr.s_addr = htonl(INADDR_ANY);

      m_Log->LogInfo("[",m_Name,"] Setting up server socket on port: ", m_Port );

      if (SOCKET_ERROR == bind(m_ServerSock, (struct sockaddr*) &m_server_addr, sizeof(m_server_addr)))
      {
         m_Log->LogError("[",m_Name,"] Unable to bind socket");
         return false;
      }

      // To listen on a socket
      if (listen(m_ServerSock, SOMAXCONN) == SOCKET_ERROR)
      {
         m_Log->LogError("[",m_Name,"] Listen failed...");
         close(m_ServerSock);
         m_ServerSock = INVALID_SOCKET;
         return false;
      }

      m_Log->LogDebug("Listening for client...");
      m_ConnectionState = ISocket::ConnectionState_t::STATE_SERVER_PORT_SETUP;
   }
   else if (m_ConnectionMode == CONN_MODE_CLIENT)
   {
      if (!reconnect())
      {
         m_ConnectionState = ISocket::ConnectionState_t::STATE_NO_CONNECTION;
         return false;
      }
      else
      {
         m_ConnectionState = ISocket::ConnectionState_t::STATE_CONNECTED;
      }
   }

   return true;
}

bool LinuxSocket::ListenForClient(std::string& client_IP)
{
   if ((m_ConnectionMode == CONN_MODE_SERVER) && (m_ServerSock != INVALID_SOCKET))
   {
      struct sockaddr_in client_addr;
      unsigned int addrlen = sizeof(client_addr);

      // Accept a client socket
      m_ClientSock = accept(m_ServerSock, (sockaddr*)&client_addr, &addrlen);

      if (m_ClientSock != INVALID_SOCKET)
      {
         fcntl(m_ClientSock, F_SETFL, O_NONBLOCK);

         char connected_ip[INET_ADDRSTRLEN];
         inet_ntop(AF_INET, &(m_client_addr.sin_addr), connected_ip, INET_ADDRSTRLEN);
         int port = ntohs(client_addr.sin_port);
         std::stringstream ss;
         ss << connected_ip << ":" << port;
         client_IP = ss.str();

         m_ConnectionState = ISocket::ConnectionState_t::STATE_CONNECTED;
      }
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
   // not implemented on linux socket
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
   int     socketError __attribute__((unused));
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
            m_ConnectionState = ISocket::ConnectionState_t::STATE_NO_CONNECTION;
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
               if (numRead >= buffer_length) // should never be greater since we read 1 byte at a time
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

bool LinuxSocket::readBlock(char* rcvBuffer, int buffer_length, int& numRead, double timeout_s __attribute__((unused)) )
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
            switch (errno)
            {
            case EINTR:
               break;

            case EAGAIN:
               usleep(250);
               numRead = 0;
               break;

            default:
               m_Log->LogError("recv failed: ", strerror(errno));
               retVal = false;
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

      if (!reconnect())
      {
          m_ConnectionState = STATE_NO_CONNECTION;
          return false;
      }
      else
      {
          m_ConnectionState = STATE_CONNECTED;
      }
   }
   else if (m_ConnectionMode == ISocket::CONN_MODE_SERVER)
   {
      m_Log->LogDebug("Socket port set to CONN_MODE_SERVER");
      if (m_Port <= 0)
      {
         m_Log->LogError("ISocket::NO_LISTENER_PORT_SPECIFIED");
         return false;
      }
      m_Log->LogDebug("Listening for client...");
      m_ConnectionState = STATE_SERVER_PORT_SETUP;
   }

   return true;
}

bool LinuxSocket::CloseConnection()
{
   if (m_ClientSock != INVALID_SOCKET)
   {
      m_Log->LogDebug("Closing open client socket -", m_ClientSock);
      close(m_ClientSock);
      m_ClientSock = INVALID_SOCKET;
   }

   if (m_ServerSock != INVALID_SOCKET)
   {
      m_Log->LogDebug("Closing open server socket - ", m_ServerSock);
      close(m_ServerSock);
      m_ServerSock = INVALID_SOCKET;
   }

   return true;
}

bool LinuxSocket::getConnectionState(ISocket::ConnectionState_t &state)
{
   state = m_ConnectionState;
   return true;
}

std::string LinuxSocket::GetName()
{
   return m_Name;
}

bool LinuxSocket::reconnect()
{
   struct sockaddr_in addr;
   struct linger li;
   int retval;

   if (-1 != m_ClientSock)
   {
      close(m_ClientSock);
      m_ClientSock = -1;
   }

   m_Log->LogDebug("[",m_Name,"] Using ", m_IPAddress, ":", m_Port);

   /*
    * Create the socket and make it be non-blocking
    */
   m_ClientSock = socket(AF_INET, SOCK_STREAM, 0);
   if (m_ClientSock == -1)
   {
      m_Log->LogError("[",m_Name,"] Unable to open port, errno: %s", strerror(errno));
      return false;
   }

   int yes = 1;
   if (-1 == setsockopt(m_ClientSock, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(socklen_t)))
   {

      m_Log->LogError("[",m_Name,"] setsocketopt failed");
      close(m_ClientSock);
      m_ClientSock = -1;

      m_Log->LogError("[",m_Name,"] Unable to open port, errno: %s", strerror(errno));
      return false;
   }

   /* Force the socket to abort when closed even if there is undelivered data */
   li.l_onoff = 1;
   li.l_linger = 0;
   setsockopt(m_ClientSock, SOL_SOCKET, SO_LINGER, (char*) &li, sizeof(struct linger));

   /*
    * Prepare to connect to the remote device
    */
   memset(&addr, '\0', sizeof(struct sockaddr_in));
   addr.sin_family = AF_INET;
   //addr.sin_port = htons(atoi(port.str().c_str()));
   addr.sin_port = htons(m_Port);
   inet_aton(m_IPAddress.c_str(), &addr.sin_addr);

#if 0
   socklen = sizeof(sockerr);
   retval = getsockopt(m_ClientSock, SOL_SOCKET, SO_ERROR, &sockerr, &socklen);
   if (retval == -1)
   {
      close(m_ClientSock);
      m_ClientSock = -1;

      LOG_ERROR("(%s) getsockopt failed, errno: %s", Name, strerror(errno));
      EXIT(c, Name);
      return false;
   }
   if (sockerr != 0)
   {
      LOG_ERROR("(%s) Socket error before connect, sockerr: %d, %s", Name, sockerr, strerror(sockerr));
   }
#endif

   /*
    * Try to establish the connection
    */
   m_Log->LogDebug("[",m_Name,"] Connecting ...");

   retval = connect(m_ClientSock, (struct sockaddr*) &addr, sizeof(struct sockaddr));

   if (retval != 0)
   {
      //LOG_ERROR("(%s) Unable to open port, errno: %s", Name, strerror(errno));
      return false;
   }

   m_Log->LogInfo("[",m_Name,"] Client connected: ", inet_ntoa(addr.sin_addr), ":", ntohs(addr.sin_port));
   fcntl(m_ClientSock, F_SETFL, O_NONBLOCK);
   return true;
}

