/**************************************************************************
*
*		     Source:  SocketTransport.h
*		    Project:  ScorpionServer
*
*		     Author: trafferty
*		       Date: Jan 27, 2015
*
*		Description: This is the transport interface for sockets.
*
****************************************************************************/
#ifndef __SOCKET_TRANSPORT_INTERFACE_H__
#define __SOCKET_TRANSPORT_INTERFACE_H__

#include <string>
#include <memory>
#include <vector>

#include "Logger.h"
#include "ISocket.h"
#include "Callback.h"

#define TX	0
#define RX 	1

#define MAX_STRING_SIZE 128

//=============================================================================
// CLASS: Socket Transport Interface Class
//-----------------------------------------------------------------------------
//=============================================================================
class SocketTransport
{
    //--------------------------------------------------------------------------
    // PUBLIC
    //--------------------------------------------------------------------------
public:

    /* Socket read modes.. */
    enum SocketReadMode_t
    {
        SOCKET_READ_MODE_LINE = 0,
        SOCKET_READ_MODE_BLOCK,
        SOCKET_READ_MODE_UNTIL
    };

             SocketTransport(bool debug = false);
    virtual ~SocketTransport();

    virtual bool UseSocket(const std::shared_ptr<ISocket> socket);

    bool init(); 

    virtual bool ResetConnection();
    virtual bool CloseConnection();

    // If there is not a thread then call this function to update.
    virtual bool doWork();

    // TransmitData: Returns TRUE is data is successfully submitted for transport.
    //               Returns FALSE if it fails.
    virtual bool TransmitData(unsigned char *data_buffer, unsigned int data_size);

    bool StartComm();
    bool StopComm();

    virtual bool RegisterRecvCallback(int callbackID, ICallback* callbackPtr);

    //virtual bool TransmitFrame(unsigned char *data_frame, unsigned int data_size);

    //--------------------------------------------------------------------------
    // PROTECTED
    //--------------------------------------------------------------------------
protected:
    struct ThreadHelper
    {
        pthread_t		thread_handle;
        pthread_attr_t	thread_attrib;

        SocketTransport	*obj;
        bool 	(SocketTransport::*method)(void);

        bool	Done;
        int    SleepTime_us;
    };


    bool m_Debug;
    std::string m_Name;
    std::shared_ptr<Logger> m_Log;
    long long m_Invoke_Cnt;

    enum CommState_t
    {
        COMM_STATE_NO_CLIENT = 0, COMM_STATE_LISTENING, COMM_STATE_CONNECTED
    };

    bool			       m_CommStarted;
    SocketReadMode_t  m_ReceiveMode;
    bool			       m_StopOnDisconnect;
    float			    m_ReceiveTimeout;
    unsigned int	    m_ReadBlockSize;
    char*	          m_ReadBuffer;
    unsigned int	    m_ReadBufferSize;
    CommState_t		 m_CommunicationState;

    //char m_LastErrorString[MAX_STRING_SIZE];

    std::shared_ptr<ISocket> m_Socket;
    ISocket::ConnectionMode_t m_ConnMode;

    std::string m_CmdBuffer;

    //void *m_TransmitQueue;
    //void *m_ReceiveQueue;
    //void *m_CallbackList;

    ICallback* m_RecvCallbackPtr;
    ICallback* m_CheckDoneCallbackPtr;

    ThreadHelper      m_ThreadHelper[2];
    // STATIC 
    static void *thread_func(void *args);

    virtual bool update_tx();
    virtual bool update_rx();

};

#endif
