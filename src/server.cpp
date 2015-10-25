// from system:
#include <unistd.h>  // for usleep
#include <sstream>
#include <memory>
#include <signal.h>

// from local:
#include "compileStats.h"
#include "CommandProcessor.h"
#include "LinuxSocket.h"
#include "SocketTransport.h"

// from common:
#include "CNT_JSON.h"
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

    /* Register a handler for control-c */
    signal(SIGINT, sigint_handler);

    std::shared_ptr<Logger> m_Log = std::shared_ptr<Logger>(new Logger("Main", true));

    std::string build_stats = buildStats();
    m_Log->LogDebug("Loading server...");
    m_Log->LogDebug("Build stats : \n", build_stats);

    cJSON *config;
    if (argc >= 2)
    {
        string filename(argv[1]);
        if (!readJSONFromFile(&config, filename))
        {
            m_Log->LogError("Error! Could not read config file: ", filename);
            return 1;
        }
    }
    else
    {
        m_Log->LogError("Must pass config file as argument");
        return 1;
    }

    m_Log->LogDebug("Loading config:");
    printJSON(config);

    bool debug = false;
    getAttributeValue_Bool(config, "debug", debug);
    cJSON *cmd_config = cJSON_GetObjectItem(config, "CommandProcessor");

    std::shared_ptr<CommandProcessor> cmd = std::shared_ptr<CommandProcessor>(new CommandProcessor(debug));

    if (cmd->init(cmd_config) == false)
    {
       m_Log->LogError("Command Processor initialization failed");
       exit(1);
    }

    cmd->Start();

    while (!CtrlC && cmd->IsRunning())
    {
       if (!cmd->doWork())
          usleep(1 * 1000);
    }

    m_Log->LogInfo("Shutting down and exiting...");

    cmd->Shutdown();

    cJSON_Delete(config);
    return 0;
}
