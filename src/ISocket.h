/**************************************************************************
*
*		     Source:  ISocket.h
*		    Project:  ScorpionServer
*
*		     Author: trafferty
*		       Date: Jan 27, 2015
*
*		Description:
*		  > interface class for generic socket implementation
*
****************************************************************************/

#ifndef  ISocket_H
#define  ISocket_H


class  ISocket
{
public:
    /* Mode */
    enum ConnectionMode_t
    {
        CONN_MODE_CLIENT = 0, CONN_MODE_SERVER
    };

    /* States */
    enum ConnectionState_t
    {
        STATE_UNKNOWN = 0,
        STATE_NO_CONNECTION,
        STATE_SERVER_PORT_SETUP,
        STATE_SERVER_LISTENING,
        STATE_CONNECTED
    };

    virtual bool Init(ConnectionMode_t mode, const std::string IPAddress, const int port) = 0; 

    virtual bool readLine(char* rcvBuffer, int buffer_length, int& numRead, double timeout_s = 1.0, bool stopOnDisconnect = false) = 0;
    virtual bool readBlock(char* rcvBuffer, int buffer_length, int& numRead, double timeout_s = 1.0) = 0;
    //bool virtual readUntil(unsigned char* bytes, unsigned int numRequested, int& numread, ICallback* RecvCallbackPtr, double timeoutSecs) = 0;
    //virtual bool readUntilToken(char* bytes, unsigned int numRequested, int& numread, const char* tokStr, double timeoutSecs) = 0;

    virtual bool sendData(const char* data, int numBytes) = 0;

    virtual bool ListenForClient(std::string& client_IP) = 0;

    virtual bool ListenForTraffic() = 0;

    virtual bool ResetConnection () = 0;
    virtual bool CloseConnection () = 0;


    virtual bool getConnectionState(ConnectionState_t &state ) = 0;

};

#endif
