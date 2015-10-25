/**************************************************************************
*
*		     Source:   CommandProcessor.h
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

#include "Callback.h"
#include "Logger.h"
#include "ISocket.h"
#include "LinuxSocket.h"
#include "SocketTransport.h"
#include "CNT_JSON.h"
#include "payload.pb.h"

class CommandProcessor
{
public:
    CommandProcessor(bool debug = false);
    virtual ~CommandProcessor(void);

    bool init(cJSON* config);

    void SetBuildStats(std::string buildStats);

    bool Start();
    bool doWork();
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
        int     SleepTime_us;
    };

    bool m_Debug;
    std::string m_Name;
    std::shared_ptr<Logger> m_Log;

    bool m_Running;
    bool m_exit_on_quit;

    std::string m_buildStats;

    pthread_mutex_t m_Working_CommandFIFO;
    pthread_mutex_t m_Working_Program;
    pthread_mutex_t m_Working_Results;

    std::shared_ptr<ISocket> m_Socket;
    std::shared_ptr<SocketTransport> m_Transport;

    std::shared_ptr<sandbox::Response_Result> m_latestResult;
    std::shared_ptr<sandbox::Response> m_response;

    Callback2<CommandProcessor, bool, intptr_t, void* >* m_callback;
    ICallback* m_ICallbackPtr;
    //bool CommandProcessor::recvCBRoutine(int replyID, void* strCBMsg);
    bool recvCBRoutine(intptr_t replyID, void* strCBMsg);

    ThreadHelper m_ThreadHelper;

    bool processCommands();
    bool programLoop();
    std::deque<std::shared_ptr<sandbox::Command> > m_CmdFIFO;
    sandbox::Command decodeToCmd(char* buffer);
    std::string encodeResponse(sandbox::Response );

    // STATIC 
    static void *thread_func(void *args);

};
#endif
