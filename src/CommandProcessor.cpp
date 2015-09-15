#include "stdafx.h"
#include "CommandProcessor.h"
#include "tinyxml2.h"

CommandProcessor::CommandProcessor(bool debug) :
m_Name("CommandProcessor"),
m_Socket(NULL),
m_Transport(NULL),
m_XaarCmdAPI(NULL),
m_buildStats(""),
m_Running(false),
m_exit_on_quit(true),
m_Debug(debug)
{
    m_Log = std::shared_ptr<Logger>(new Logger(m_Name, m_Debug));

    pthread_mutex_init(&m_Working_FIFO, NULL);

    m_callback = new Callback2<CommandProcessor, bool, int, void*>(this, &CommandProcessor::recvCBRoutine, 0, 0);
    m_ICallbackPtr = m_callback;
}

CommandProcessor::~CommandProcessor(void)
{
}

bool CommandProcessor::Init(cJSON* config)
{
    string ipAddress;
    int port;

    if (!getAttributeValue_String(config, "ipAddress", ipAddress) ||
        !getAttributeValue_Int(config, "port", port))
    {
        m_Log->LogError("Could not get ipAddress or port from config: ");
        printJSON(config);
        return false;
    }

    cJSON* XML_Files = cJSON_GetObjectItem(config, "XML_Files");
    if (XML_Files != NULL)
    {
        for (int idx = 0; idx < cJSON_GetArraySize(XML_Files); idx++)
        {
            cJSON *XML_file = cJSON_GetArrayItem(XML_Files, idx)->child;
            m_XML_Files.insert(make_pair(XML_file->string, XML_file->valuestring));
        }
    }

    if (!getAttributeValue_Bool(config, "exit_on_quit", m_exit_on_quit))
    {
        m_exit_on_quit = true;
    }

    m_Socket = std::shared_ptr<ISocket>(new LinuxSocket("ClientSocket", m_Debug));
    if (m_Socket->Init(ISocket::ConnectionMode_t::CONN_MODE_SERVER, ipAddress.c_str(), port) == false)
    {
        m_Log->LogError("Socket initialization failed");
        return false;
    }

    //m_Socket->RegisterRecvCallback(23, m_ICallbackPtr);

    m_Transport = std::shared_ptr<SocketTransport>(new SocketTransport(m_Debug));
    m_Transport->UseSocket(std::static_pointer_cast<ISocket>(m_Socket));
    if (m_Transport->Init() == false)
    {
        m_Log->LogError("Transport initialization failed");
        return false;
    }

    m_Transport->RegisterRecvCallback(23, m_ICallbackPtr);


    cJSON* XaarCmdAPI_config = cJSON_GetObjectItem(config, "XaarCmdAPI");
    if (XaarCmdAPI_config == NULL)
    {
        XaarCmdAPI_config = cJSON_CreateObject();
    }

    m_XaarCmdAPI = std::shared_ptr<XaarCmdAPI>(new XaarCmdAPI(m_Debug));
    if (m_XaarCmdAPI->Init(XaarCmdAPI_config) == false)
    {
        m_Log->LogError("Xaar CmdAPI initialization failed");
        return false;
    }

    return true;
}

bool CommandProcessor::recvCBRoutine(int replyID, void* CBMsg)
{
    std::string &strCBMsg = *static_cast<std::string*>(CBMsg);
    //string strCBMsg(CBMsg);

    m_Log->LogDebug("Reply ID: ", replyID, " msg size: ", strCBMsg.length());

    // push on the FIFO
    pthread_mutex_lock(&m_Working_FIFO);
    {
        m_CmdFIFO.push_back(strCBMsg);
    }
    pthread_mutex_unlock(&m_Working_FIFO);

    return true;
}

bool CommandProcessor::Start()
{
    if (m_Transport->StartComm() == false)
    {
        m_Log->LogError("Transport initialization failed");
        return false;
    }

    m_ThreadHelper.method = &CommandProcessor::ProcessCommands;
    m_ThreadHelper.obj = (this);
    m_ThreadHelper.Done = false;
    m_ThreadHelper.SleepTime_ms = 1;

    // create the RX Thread
    if (pthread_create(&(m_ThreadHelper.thread_handle), 0,
        CommandProcessor::thread_func, (void *)&m_ThreadHelper) != 0)
    {
        m_Log->LogError("Error spawning ProcessCommands thread");
        return false;
    }

    m_Running = true;

    return true;
}

bool CommandProcessor::IsRunning()
{
    return m_Running;
}

bool CommandProcessor::Shutdown()
{
    m_Running = false;
    m_ThreadHelper.Done = true;
    m_Log->LogDebug("ProcessCommands thread stopped, shutting down comms...");

    m_Transport->StopComm();

    m_Log->LogDebug("Waiting for join...");
    pthread_join(m_ThreadHelper.thread_handle, NULL);

    return true;
}


bool CommandProcessor::ProcessCommands()
{
    std::string newCmd;
    RPCObj RPC_Obj = NULL;
    RPC_RespObj resp_obj = NULL;

    if (!m_Running)
        return false;

    pthread_mutex_lock(&m_Working_FIFO);
    {
        if (m_CmdFIFO.empty())
        {
            pthread_mutex_unlock(&m_Working_FIFO);
            return false;
        }
        newCmd = m_CmdFIFO.front();
        m_CmdFIFO.pop_front();
    }
    pthread_mutex_unlock(&m_Working_FIFO);

    // Create an array to hold log msgs...will be printed out by client (XPM)
    cJSON *log_msgs_array = cJSON_CreateArray();

    // First see if newCmd is the 'quit' cmd
    if ((newCmd.find("quit") != std::string::npos))
    {
        if (m_exit_on_quit)
        {
            m_Log->LogDebug("Received quit cmd...shutting down...");
            m_Running = false;
        }

        RPC_Obj = createRPCObj("quit", 0);
        resp_obj = createRPC_RespObj(RPC_Obj);
        cJSON_AddItemToArray(log_msgs_array, cJSON_CreateString("ScorpionServer shutting down..."));
    }
    else
    {
        RPC_Obj = cJSON_Parse((const char *)newCmd.c_str());

        if (RPC_Obj != NULL)
        {
            string method;
            if (getAttributeValue_String(RPC_Obj, "method", method))
            {
                if (method.find("setRegistrySetting") != std::string::npos)
                {
                    resp_obj = setRegistrySetting(RPC_Obj, log_msgs_array);
                }
                else if (method.find("getRegistrySetting") != std::string::npos)
                {
                    resp_obj = getRegistrySetting(RPC_Obj, log_msgs_array);
                }
                else if (method.find("getBuildStats") != std::string::npos)
                {
                    resp_obj = getBuildStats(RPC_Obj);
                }
                else
                {
                    resp_obj = m_XaarCmdAPI->processCommand(RPC_Obj, log_msgs_array);
                }

                if (resp_obj == NULL)
                {
                    resp_obj = createRPC_RespObj(RPC_Obj);
                    RPC_Result result_obj = cJSON_CreateObject();
                    cJSON_AddNumberToObject(result_obj, "error_code", -991);
                    cJSON_AddFalseToObject(result_obj, "Success");
                    cJSON_AddItemToObject(resp_obj, "result", result_obj);
                    cJSON_AddItemToArray(log_msgs_array, cJSON_CreateString("Error in RPC method"));
                    m_Log->LogError("Error in RPC method");
                }
            }
            else
            {
                resp_obj = createRPC_RespObj(RPC_Obj);
                RPC_Result result_obj = cJSON_CreateObject();
                cJSON_AddNumberToObject(result_obj, "error_code", -992);
                cJSON_AddFalseToObject(result_obj, "Success");
                cJSON_AddItemToObject(resp_obj, "result", result_obj);
                cJSON_AddItemToArray(log_msgs_array, cJSON_CreateString("No method listed in RPC"));
                m_Log->LogError("No method listed in RPC");
                printJSONUnformatted(RPC_Obj);
            }
        }
        else
        {
            RPC_Obj = createRPCObj("unknown", 0);
            resp_obj = createRPC_RespObj(RPC_Obj);
            RPC_Result result_obj = cJSON_CreateObject();
            cJSON_AddNumberToObject(result_obj, "error_code", -993);
            cJSON_AddFalseToObject(result_obj, "Success");
            cJSON_AddItemToObject(resp_obj, "result", result_obj);
            cJSON_AddItemToArray(log_msgs_array, cJSON_CreateString("Invalid RPC message; could not parse"));

            m_Log->LogError("Invalid RPC message; could not parse: ", newCmd.c_str());
        }
    }

    if (resp_obj != NULL)
    {
        cJSON_AddItemToObject(resp_obj, "log_msgs", log_msgs_array);

        stringstream ss;
        char* tmpStr = cJSON_PrintUnformatted(resp_obj);
        ss << tmpStr;
        ss << '\n';
        m_Socket->sendData(ss.str().c_str(), (int)ss.str().length());
        free(tmpStr);

        cJSON_Delete(resp_obj);
    }
    else
    {
        cJSON_Delete(log_msgs_array);
    }

    cJSON_Delete(RPC_Obj);

    //m_Log->LogDebug("Processing complete.");

    return true;
}


RPC_RespObj CommandProcessor::setRegistrySetting(RPCObj RPC_Obj, cJSON* log_msgs_array)
{
    RPC_RespObj resp_obj = NULL;
    RPC_Result result_obj = NULL;
    RPC_Params  params = NULL;
    std::string path_name = "";
    std::string key_name = "";
    int value;
    bool createKey;

    if (doesAttributeExist(RPC_Obj, "params")) {
        params = cJSON_GetObjectItem(RPC_Obj, "params");
    }
    else {
        m_Log->LogError("Error in RPC: No params");
        return resp_obj;
    }
    if (doesAttributeExist(params, "path_name")) {
        path_name = cJSON_GetObjectItem(params, "path_name")->valuestring;
    }
    else {
        m_Log->LogError("Error in RPC: No path_name key");
        return resp_obj;
    }
    if (doesAttributeExist(params, "key_name")) {
        key_name = cJSON_GetObjectItem(params, "key_name")->valuestring;

        // remove all the spaces and equal signs
        key_name.erase(std::remove(key_name.begin(), key_name.end(), ' '), key_name.end());
        key_name.erase(std::remove(key_name.begin(), key_name.end(), '='), key_name.end());
        key_name.erase(std::remove(key_name.begin(), key_name.end(), ','), key_name.end());
    }
    else {
        m_Log->LogError("Error in RPC: No key_name key");
        cJSON_AddItemToArray(log_msgs_array, cJSON_CreateString("Error in RPC: No key_name key"));
        return resp_obj;
    }
    if (doesAttributeExist(params, "value")) {
        value = cJSON_GetObjectItem(params, "value")->valueint;
    }
    else {
        m_Log->LogError("Error in RPC: No value key");
        cJSON_AddItemToArray(log_msgs_array, cJSON_CreateString("Error in RPC: No value key"));
        return resp_obj;
    }
    if (doesAttributeExist(params, "createKey")) {
        createKey = (cJSON_GetObjectItem(params, "createKey")->type == cJSON_True ? true : false);
    }
    else {
        m_Log->LogError("Error in RPC: No createKey key");
        cJSON_AddItemToArray(log_msgs_array, cJSON_CreateString("Error in RPC: No createKey key"));
        return resp_obj;
    }

    //m_Log->LogError("****************** Processing setRegistrySetting:");
    //printJSONUnformatted(RPC_Obj);

    // parse the path_name into elements, ie, "SOFTWARE\\Xaar\\XUSB\\Common" -> [SOFTWARE, Xaar, XUSB, Common]
    std::vector<std::string> elems;
    std::stringstream ss(path_name);
    std::string item;
    while (std::getline(ss, item, '\\')) {
        elems.push_back(item);
    }

    std::string root(elems[2]);
    std::string section(elems[3]);
    // sanitize the section...
    section.erase(std::remove(section.begin(), section.end(), ' '), section.end());
    section.erase(std::remove(section.begin(), section.end(), '='), section.end());
    section.erase(std::remove(section.begin(), section.end(), ','), section.end());

    /*
    "params" : {"path_name":"SOFTWARE\\Xaar\\XUSB\\Common", "key_name" : "EncoderDivide", "value" : 47, "createKey" : true}}
    "params" : {"path_name":"SOFTWARE\\Xaar\\XUSB\\Common", "key_name" : "ForceBinary = 1, GreyScale = 2", "value" : 1, "createKey" : true}}
    "params" : {"path_name":"SOFTWARE\\Xaar\\XUSB\\Configuration", "key_name" : "SeparateRows", "value" : 0, "createKey" : true}}
    "params" : {"path_name":"SOFTWARE\\Xaar\\XUSB\\HeadOffsetRegisters Card 1", "key_name" : "Mirror Head 1 Row 1", "value" : 0, "createKey" : true}}

    <Common>
    <ImageDir type="string" value="C:\Program Files\Xaar\Scorpion\Images" />
    <HelpDirectory type="string" value="C:\Program Files\Xaar\Scorpion" />
    <EncoderDivide type="integer" value="18" />
    <SubPixelDivide type="integer" value="0" />
    </Common>
    */

    tinyxml2::XMLDocument doc;
    doc.LoadFile(m_XML_Files["XUSB"].c_str());
    if (doc.ErrorID() == 0)
    {
        tinyxml2::XMLHandle docHandle(&doc);
        tinyxml2::XMLElement* child = docHandle.FirstChildElement(root.c_str()).FirstChildElement(section.c_str()).FirstChildElement(key_name.c_str()).ToElement();
        if (child)
        {
            std::stringstream logMsg;
            logMsg << "setRegistrySetting: section: " << section << ", key: " << key_name << ", value:" << value;
            m_Log->LogDebug(logMsg.str());
            cJSON_AddItemToArray(log_msgs_array, cJSON_CreateString(logMsg.str().c_str()));

            std::stringstream ss;
            ss << value;
            child->SetAttribute("value", ss.str().c_str());
        }
        else
        {
            m_Log->LogWarn("setRegistrySetting: key not found:  ", section, ", key: ", key_name);

            if (createKey)
            {
                std::stringstream logMsg;
                logMsg << "setRegistrySetting: Creating section: " << section << ", key: " << key_name << ", value:" << value;
                m_Log->LogWarn(logMsg.str());
                cJSON_AddItemToArray(log_msgs_array, cJSON_CreateString(logMsg.str().c_str()));
                tinyxml2::XMLNode* node = docHandle.FirstChildElement(root.c_str()).FirstChildElement(section.c_str()).ToNode();
                if (node == NULL)
                {
                    std::stringstream logMsg;
                    logMsg << "setRegistrySetting: Error Creating key!! Section: " << section << ", key: " << key_name;
                    m_Log->LogError(logMsg.str());
                    cJSON_AddItemToArray(log_msgs_array, cJSON_CreateString(logMsg.str().c_str()));
                }
                else
                {
                    tinyxml2::XMLElement* child = doc.NewElement(key_name.c_str());

                    child->SetAttribute("type", "string");
                    std::stringstream ss;
                    ss << value;
                    child->SetAttribute("value", ss.str().c_str());
                    node->InsertEndChild(child);
                }
            }
        }

        doc.SaveFile(m_XML_Files["XUSB"].c_str());

        resp_obj = createRPC_RespObj(RPC_Obj);
        result_obj = cJSON_CreateObject();
        cJSON_AddTrueToObject(result_obj, "Success");
        cJSON_AddItemToObject(resp_obj, "result", result_obj);
    }

    return resp_obj;
}

RPC_RespObj CommandProcessor::getRegistrySetting(RPCObj RPC_Obj, cJSON* log_msgs_array)
{
    RPC_RespObj resp_obj = NULL;
    RPC_Result result_obj = NULL;

    std::stringstream logMsg;
    logMsg << "getRegistrySetting not yet implemented...";
    m_Log->LogWarn(logMsg.str());
    cJSON_AddItemToArray(log_msgs_array, cJSON_CreateString(logMsg.str().c_str()));

    resp_obj = createRPC_RespObj(RPC_Obj);
    result_obj = cJSON_CreateObject();
    cJSON_AddFalseToObject(result_obj, "Success");
    cJSON_AddItemToObject(resp_obj, "result", result_obj);

    return resp_obj;
}

RPC_RespObj CommandProcessor::getBuildStats(RPCObj RPC_Obj)
{
    RPC_RespObj resp_obj = NULL;
    RPC_Result result_obj = NULL;

    std::stringstream ss(m_buildStats);
    std::string build_stat;
    cJSON* build_stats_array = cJSON_CreateArray();

    while (std::getline(ss, build_stat, '\n'))
    {
        cJSON_AddItemToArray(build_stats_array, cJSON_CreateString(build_stat.c_str()));
    }
    resp_obj = createRPC_RespObj(RPC_Obj);
    result_obj = cJSON_CreateObject();
    cJSON_AddTrueToObject(result_obj, "Success");
    cJSON_AddItemToObject(result_obj, "build_stats", build_stats_array);
    cJSON_AddItemToObject(resp_obj, "result", result_obj);

    return resp_obj;
}

void CommandProcessor::SetBuildStats(std::string buildStats)
{
    m_buildStats = buildStats;
}


//=============================================================================
// thread_func
//-----------------------------------------------------------------------------
//=============================================================================
void *CommandProcessor::thread_func(void *args)
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
