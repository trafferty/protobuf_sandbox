/**************************************************************************
*
*          Source:   CommandProcessor.h
*
*          Author: trafferty
*            Date: Jan 27, 2015
*
*     Description:
*       > Command Processing Engine
*
****************************************************************************/

#include <unistd.h>
#include <ctime>    // for std::time()
#include <cstdlib>  // for std::rand()

#include "CommandProcessor.h"

#include <google/protobuf/message.h>
#include <google/protobuf/descriptor.h>
#include <google/protobuf/io/zero_copy_stream_impl.h>
#include <google/protobuf/io/coded_stream.h>
#include <google/protobuf/io/zero_copy_stream_impl_lite.h>

using namespace google::protobuf::io;


CommandProcessor::CommandProcessor(bool debug) :
   m_Debug(debug),
   m_Name("CommandProcessor"),
   m_Running(false),
   m_exit_on_quit(true),
   m_buildStats(""),
   m_Socket(nullptr),
   m_Transport(nullptr),
   m_latestResult(NULL)
{
   m_Log = std::shared_ptr<Logger>(new Logger(m_Name, m_Debug));

   pthread_mutex_init(&m_Working_CommandFIFO, NULL);
   pthread_mutex_init(&m_Working_Program, NULL);
   pthread_mutex_init(&m_Working_Results, NULL);

   m_callback = new Callback2<CommandProcessor, bool, intptr_t, void*>(this, &CommandProcessor::recvCBRoutine, 0, 0);
   m_ICallbackPtr = m_callback;

   m_latestResult = std::shared_ptr<sandbox::Response_Result>(new sandbox::Response_Result());
   m_response     = std::shared_ptr<sandbox::Response>(new sandbox::Response());
}

CommandProcessor::~CommandProcessor(void)
{
}

bool CommandProcessor::init(cJSON* config)
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

   if (!getAttributeValue_Bool(config, "exit_on_quit", m_exit_on_quit))
   {
      m_exit_on_quit = true;
   }

   m_Socket = std::shared_ptr<ISocket>(new LinuxSocket("ClientSocket", m_Debug));
   if (m_Socket->init(ISocket::ConnectionMode_t::CONN_MODE_SERVER, ipAddress.c_str(), port) == false)
   {
      m_Log->LogError("Socket initialization failed");
      return false;
   }

   //m_Socket->RegisterRecvCallback(23, m_ICallbackPtr);

   m_Transport = std::shared_ptr<SocketTransport>(new SocketTransport(m_Debug));
   m_Transport->UseSocket(std::static_pointer_cast<ISocket>(m_Socket));
   if (m_Transport->init() == false)
   {
      m_Log->LogError("Transport initialization failed");
      return false;
   }

   m_Transport->RegisterRecvCallback(23, m_ICallbackPtr);


   cJSON* imgEngine_config = cJSON_GetObjectItem(config, "imgEngine");
   if (imgEngine_config == NULL)
   {
      m_Log->LogError("Could not get imgEngine from config: ");
      printJSON(config);
      return false;
   }

   string imgEng_type;

   if (!getAttributeValue_String(imgEngine_config, "imgEng_type", imgEng_type))
   {
      m_Log->LogError("Could not get imgEng_type from config: ");
      printJSON(config);
      return false;
   }

   m_Log->LogInfo("CommandProcessor initialized...");

   return true;
}

bool CommandProcessor::recvCBRoutine(intptr_t replyID, void* CBMsg)
{
   //char* buffer = static_cast<char*>(CBMsg);
   //sandbox::Command cmd = decodeBuffer(buffer);

   std::shared_ptr<sandbox::Command> cmd = std::shared_ptr<sandbox::Command>(new sandbox::Command);

   std::string &strCBMsg = *static_cast<std::string*>(CBMsg);

   cmd->ParseFromString(strCBMsg);

   //std::string &strCBMsg = *static_cast<std::string*>(CBMsg);
   //string strCBMsg(CBMsg);

   m_Log->LogDebug("Reply ID: ", replyID, " msg size: ", cmd->ByteSize(), "->", cmd->DebugString());

   // push on the FIFO
   pthread_mutex_lock(&m_Working_CommandFIFO);
   {
      m_CmdFIFO.push_back(cmd);
   }
   pthread_mutex_unlock(&m_Working_CommandFIFO);

   return true;
}

sandbox::Command decodeBuffer(char* buffer  __attribute__((unused)))
{
   sandbox::Command cmd;
//   int bytecount=0;
//
//   google::protobuf::uint32 size;
//   google::protobuf::io::ArrayInputStream ais(buffer, 4);
//   CodedInputStream coded_input(&ais);
//   coded_input.ReadVarint32(&size);//Decode the HDR and get the size
//   cout << "size of payload is " << size << endl;
//
//   //Assign ArrayInputStream with enough memory
//   google::protobuf::io::ArrayInputStream ais(buffer, size+4);
//   CodedInputStream coded_input(&ais);
//
//   //Read an unsigned integer with Varint encoding, truncating to 32 bits.
//   coded_input.ReadVarint32(&size);
//
//   //After the message's length is read, PushLimit() is used to prevent the CodedInputStream
//   //from reading beyond that length.Limits are used when parsing length-delimited
//   //embedded messages
//   google::protobuf::io::CodedInputStream::Limit msgLimit = coded_input.PushLimit(size);
//
//   //De-Serialize
//   cmd.ParseFromCodedStream(&coded_input);
//
//   //Once the embedded message has been parsed, PopLimit() is called to undo the limit
//    coded_input.PopLimit(msgLimit);
//
   return cmd;
}

bool CommandProcessor::doWork()
{
   if (m_Running)
      return processCommands();
   else
      return false;
}

bool CommandProcessor::Start()
{
   if (m_Transport->StartComm() == false)
   {
      m_Log->LogError("Transport initialization failed");
      return false;
   }

   m_Log->LogDebug("Using internal worker thread...");

   m_Log->LogDebug("Initializing random generator...");
   std::srand(std::time(0)); // use current time as seed for random generator

   m_ThreadHelper.method = &CommandProcessor::programLoop;
   m_ThreadHelper.obj = (this);
   m_ThreadHelper.Done = false;
   m_ThreadHelper.SleepTime_us = 500;

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

bool CommandProcessor::programLoop()
{
   pthread_mutex_lock(&m_Working_Results);
   {
      m_latestResult->set_contact_radius(std::rand()*0.76);
      m_latestResult->set_center_point(0, std::rand()*0.06);
      m_latestResult->set_center_point(1, std::rand()*0.16);
   }
   pthread_mutex_unlock(&m_Working_Results);

   usleep(10 * 1000);
   return true;
}

bool CommandProcessor::processCommands()
{
   std::shared_ptr<sandbox::Command> newCmd;

   if (!m_Running)
      return false;

   pthread_mutex_lock(&m_Working_CommandFIFO);
   {
      if (!m_CmdFIFO.empty())
      {
         newCmd = m_CmdFIFO.front();
         m_CmdFIFO.pop_front();
      }
   }
   pthread_mutex_unlock(&m_Working_CommandFIFO);

   if (newCmd->ByteSize() == 0)
   {
      m_response->set_id(newCmd->id());
      m_response->mutable_result()->set_success(sandbox::Response_Success_FALSE);
   }

   // First see if newCmd is the 'quit' cmd
   if (newCmd->method().find("quit") != std::string::npos)
   {
      if (m_exit_on_quit)
      {
         m_Log->LogDebug("Received quit cmd...shutting down...");
         m_Running = false;
      }
   }
   else if (newCmd->method().find("status") != std::string::npos)
   {
      m_response->set_id(newCmd->id());
      m_response->mutable_result()->set_status(sandbox::Response_Status_OK);
   }

   else if (newCmd->method().find("start") != std::string::npos)
   {
      m_response->set_id(newCmd->id());
      m_response->mutable_result()->set_success(sandbox::Response_Success_TRUE);
   }

   else if (newCmd->method().find("stop") != std::string::npos)
   {
      m_response->set_id(newCmd->id());
      m_response->mutable_result()->set_success(sandbox::Response_Success_TRUE);
   }

   else if (newCmd->method().find("query") != std::string::npos)
   {
      m_response->set_id(newCmd->id());

      pthread_mutex_lock(&m_Working_Results);
      {
         //m_response->mutable_result()->CopyFrom(m_latestResult);
         m_response->mutable_result()->set_contact_radius(m_latestResult->contact_radius());
         m_response->mutable_result()->set_center_point(0, m_latestResult->center_point(0));
         m_response->mutable_result()->set_center_point(1, m_latestResult->center_point(1));
      }
      pthread_mutex_unlock(&m_Working_Results);


//      double contact_radius;
//      double center_point_x;
//      double center_point_y;
//
//      pthread_mutex_lock(&m_Working_Results);
//      {
//         if (m_latestResult != NULL)
//         {
//            contact_radius = m_latestResult->contact_radius();
//            center_point_x = m_latestResult->center_point(0);
//            center_point_y = m_latestResult->center_point(1);
//         }
//         else
//         {
//            m_Log->LogDebug("No results yet, creating a fake set" );
//
//            contact_radius = -99.0;
//            center_point_x = -99.0;
//            center_point_y = -99.0;
//         }
//
//         m_response->mutable_result()->CopyFrom(m_latestResult);
//      }
//      pthread_mutex_unlock(&m_Working_Results);

   }

   else
   {
      m_response->set_id(newCmd->id());
      m_response->mutable_result()->set_success(sandbox::Response_Success_FALSE);
   }

   std::string buf;
   m_response->SerializeToString(&buf);

   m_Socket->sendData(buf.c_str(), (int)buf.length());

   return true;
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
      usleep(threadHelper->SleepTime_us);
   }

   pthread_exit(0);
   return NULL;
}
