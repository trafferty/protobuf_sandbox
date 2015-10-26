// local:
#include "Callback.h"
#include "Logger.h"
#include "ISocket.h"
#include "LinuxSocket.h"
#include "SocketTransport.h"
#include "payload.pb.h"

// from common:
#include "CNT_JSON.h"
#include "Logger.h"

// from system:
#include <sstream>
#include <memory>
#include <stdio.h>
#include <iomanip>
#include <signal.h>

#include <cstring>
#include <unistd.h>


using namespace std;

bool CtrlC = false;
void sigint_handler(int n)
{
    CtrlC = true;
    std::cerr << "sigint received - aborting: " << n << std::endl;
}

std::string readLineFromSocket(std::shared_ptr<ISocket> socket)
{
   int numRead = 0;
   const int bufSize=512;
   std::vector<char> buffer(bufSize);
   std::string line("");

   ISocket::ConnectionState_t connectionState;
   socket->getConnectionState(connectionState);

   if (connectionState == ISocket::STATE_CONNECTED)
   {
      if (socket->readLine(buffer.data(), bufSize, numRead, true))
      {
         line = std::string(buffer.data(), buffer.size());
      }
   }

   return line;
}

bool sendCommand(std::shared_ptr<ISocket> socket, std::shared_ptr<sandbox::Command> newCmd)
{
   bool ret_val;

   std::string buf;
   newCmd->SerializeToString(&buf);

   ret_val = socket->sendData(buf.c_str(), (int)buf.length());
   return ret_val;
}

int main(int argc, char* argv[])
{
   bool debug = true;
   int cmd_idx = 1;
   std::string line = "";

   /* Register a handler for control-c */
   signal(SIGINT, sigint_handler);

   std::shared_ptr<Logger> m_Log = std::shared_ptr<Logger>(new Logger("Main", true));

   string ipAddress;
   int port;
   int freq_hz;
   if (argc == 4)
   {
       ipAddress = argv[1];
       port = std::stoi(std::string(argv[2]));
       freq_hz = std::stoi(std::string(argv[3]));
   }
   else
   {
      ipAddress = "127.0.0.1";
      port = 12070;
      freq_hz = 50;
   }
   m_Log->LogInfo("Using socket settings: ", ipAddress, ":", port);

   int sleep_time_ms = int(1000/freq_hz);
   m_Log->LogInfo("Query freq (hz): ", freq_hz, ", sleep time (ms): ", sleep_time_ms);

   m_Log->LogInfo("Creating client socket, connecting to ", ipAddress, ":", port);
   std::shared_ptr<ISocket> m_Socket = std::shared_ptr<ISocket>(new LinuxSocket("ClientSocket", debug));
   if (m_Socket->init(ISocket::ConnectionMode_t::CONN_MODE_CLIENT, ipAddress.c_str(), port) == false)
   {
      m_Log->LogError("Socket initialization failed");
      return false;
   }

   std::shared_ptr<sandbox::Command> newCmd;
   newCmd->set_method("status");

   for (int i = 0; i < 10; i++, cmd_idx += i)
   {
      m_Log->LogInfo("Sending status command...");

      newCmd->set_id(cmd_idx);
      if (!sendCommand(m_Socket, newCmd))
      {
         m_Log->LogError("Error sending status command");
         return false;
      }
      // now get the response: "{ "id": 1234,  "result": {"status": "ok" } }"
      line = readLineFromSocket(m_Socket);
      if ((line.length() == 0) || (line.find("error") != std::string::npos))
      {
         m_Log->LogError("Status cmd returned error: ", line);
         return false;
      }
   }

   m_Log->LogInfo("Sending start command...");
   newCmd->set_method("start");
   newCmd->set_id(++cmd_idx);

   if (!sendCommand(m_Socket, newCmd))
   {
      m_Log->LogError("Error sending status command");
      return false;
   }
   // now get the response: "{ "id": 1235,  "result": {"success": true } }"
   line = readLineFromSocket(m_Socket);
   if ((line.length() == 0) || (line.find("false") != std::string::npos))
   {
      m_Log->LogError("Start cmd returned error...");
      return false;
   }

   int unsuccess_cnt = 0;
   while (!CtrlC && (unsuccess_cnt < 5000))
   {
      std::stringstream ss;

      ss << "Send query [" << cmd_idx << "] - ";
      //cerr << ">>>> send query" << endl;
      newCmd->set_method("query");
      newCmd->set_id(++cmd_idx);

      if (sendCommand(m_Socket, newCmd))
      {
         usleep(sleep_time_ms * 1000);
         std::string line = readLineFromSocket(m_Socket);

         //cerr << ">>>> send query reply: " << line << endl;

         if (line.length() > 0)
         {
            cJSON* resp_obj = cJSON_Parse(line.c_str());
            if (resp_obj != NULL)
            {
               int result_idx  = getAttributeDefault_Int(resp_obj, "id", -1);

               cJSON* result_obj = cJSON_GetObjectItem(resp_obj, "result");
               bool success = (cJSON_GetObjectItem(result_obj, "success")->type == cJSON_True ? true : false);

               if (success)
               {
                  unsuccess_cnt = 0;
                  double contact_radius = cJSON_GetObjectItem(result_obj, "contact_radius")->valuedouble;
                  double center_pt_x = cJSON_GetObjectItem(result_obj, "center_pt_x")->valuedouble;
                  double center_pt_y = cJSON_GetObjectItem(result_obj, "center_pt_y")->valuedouble;
                  ss << "Rcvd result [" << result_idx << "]: " << contact_radius << ", " << center_pt_x << ", " << center_pt_y;
               }
               else
               {
                  unsuccess_cnt++;
                  ss << "[" << result_idx << "]: contact radius not found (" << unsuccess_cnt << ")";
               }
               m_Log->LogInfo(ss.str());
               cJSON_Delete(resp_obj);
            }
            else
            {
               m_Log->LogError("Error parsing response: ", line);
            }
         }
      }
      usleep(sleep_time_ms * 1000);
   }

   m_Log->LogInfo("Sending stop command...");
   newCmd->set_method("stop");
   newCmd->set_id(++cmd_idx);

   if (!sendCommand(m_Socket, newCmd))
   {
      m_Log->LogError("Error sending stop command");
      return false;
   }
   // now get the response: "{ "id": 1235,  "result": {"success": true } }"
   line = readLineFromSocket(m_Socket);
   if ((line.length() == 0) || (line.find("false") != std::string::npos))
   {
      m_Log->LogError("Stop cmd returned error...");
      return false;
   }

   m_Log->LogInfo("Shutting down and exiting...");

   return 0;
}
