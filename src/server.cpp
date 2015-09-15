/**************************************************************************
*
*		     Source:  imgAPO.cpp
*		    Project:  imgAPI: Image Acquire, Process Output
*
*		     Author:  trafferty
*		       Date:  June, 3, 2015
*
****************************************************************************/
// from system:
#include <iostream>
#include <sstream>
#include <memory>
#include <signal.h>

// from local:
#include "compileStats.h"
#include "Logger.h"

using namespace std;

bool CtrlC = false;
void sigint_handler(int n)
{
    CtrlC = true;
    std::cerr << "sigint received - aborting: " << n << std::endl;
}

std::string buildStats()
{
    std::stringstream ss;
#ifdef NO_BUILD_STATS
    ss << "  Build stats not available" << std::endl;
#else
    ss << "  git-sha:      " << compiled_git_sha << std::endl;
    ss << "  git-branch:   " << compiled_git_branch << std::endl;
    ss << "  compile user: " << compiled_user << std::endl;
    ss << "  compile host: " << compiled_host << std::endl;
    ss << "  compile date: " << compiled_date << std::endl;
    ss << "  note:         " << compiled_note << std::endl;
#endif
    return ss.str();
}

int main(int argc, char* argv[])
{
    std::shared_ptr<Logger> m_Log = std::shared_ptr<Logger>(new Logger("main", true));
    m_Log->LogDebug("Hello...");

    /* Register a handler for control-c */
    signal(SIGINT, sigint_handler);


    std::string build_stats = buildStats();

    //cJSON *cmd_config = cJSON_GetObjectItem(config, "CommandProcessor");

    //~ std::shared_ptr<CommandProcessor> cmd = std::shared_ptr<CommandProcessor>(new CommandProcessor(debug));
//~
    //~ cmd->Init(cmd_config);
    //~ cmd->SetBuildStats(build_stats);
    //~ cmd->Start();
//~
    //~ while (!CtrlC && cmd->IsRunning())
    //~ {
        //~ Sleep(100);
    //~ }
//~
    //~ cmd->Shutdown();

    return 0;
}
