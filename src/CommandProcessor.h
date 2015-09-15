/**************************************************************************
*
*		     Source:   CommandProcessor.h
*		    Project:  ScorpionServer
*
*		     Author: trafferty
*		       Date: Jan 27, 2015
*
*		Description:
*		  > Command Processing Engine 
*
****************************************************************************/

#ifndef  CommandProcessor_H
#define  CommandProcessor_H

#include <string>
#include <deque>

#include "Logger.h"
#include "ISocket.h"
#include "LinuxSocket.h"
#include "SocketTransport.h"
#include "XaarCmdAPI.h"
#include "Callback.h"
#include "CNT_JSON.h"


class CommandProcessor
{
public:
    CommandProcessor(bool debug = false);
    virtual ~CommandProcessor(void);

    bool Init(cJSON* config);

    void SetBuildStats(std::string buildStats);

    bool Start();
    bool Shutdown();

    bool IsRunning();

protected:
    struct ThreadHelper
    {
        pthread_t		thread_handle;
        pthread_attr_t	thread_attrib;

        CommandProcessor	*obj;

        bool 	(CommandProcessor::*method)(void);
        bool	Done;
        int     SleepTime_ms;
    };

    bool m_Debug;
    std::string m_Name;
    std::shared_ptr<Logger> m_Log;

    bool m_Running;
    bool m_exit_on_quit;

    std::string m_buildStats;

    pthread_mutex_t m_Working_FIFO;
    std::shared_ptr<ISocket> m_Socket;
    std::shared_ptr<SocketTransport> m_Transport;
    std::shared_ptr<XaarCmdAPI> m_XaarCmdAPI;

    Callback2<CommandProcessor, bool, int, void* >* m_callback;
    ICallback* m_ICallbackPtr;
    bool CommandProcessor::recvCBRoutine(int replyID, void* strCBMsg);

    ThreadHelper m_ThreadHelper;

    bool ProcessCommands();
    std::deque<std::string> m_CmdFIFO;
    RPC_RespObj setRegistrySetting(RPCObj RPC_Obj, cJSON* log_msgs_array);
    RPC_RespObj getRegistrySetting(RPCObj RPC_Obj, cJSON* log_msgs_array);
    RPC_RespObj getBuildStats(RPCObj RPC_Obj);

    std::map<std::string, std::string> m_XML_Files;

    // STATIC 
    static void *thread_func(void *args);

};
#endif
