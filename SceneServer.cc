#include <muduo/base/Logging.h>

#include <boost/bind.hpp>

#include <muduo/net/EventLoop.h>
#include <muduo/net/TcpServer.h>
#include <muduo/net/TcpConnection.h>
#include <muduo/base/Mutex.h>

#include <map>
#include <memory>
#include <set>
#include <thread>
#include <sys/time.h>

#include "World.h"
#include "Player.h"
#include "Global.h"
///////////////////////////////


///////////////////////////////

class SceneServer
{
 public:
  typedef std::map<muduo::net::TcpConnectionPtr, PlayerPtr> MapConnections;

  SceneServer(muduo::net::EventLoop* loop,
             const muduo::net::InetAddress& listenAddr, World* world, int threadNum);

  void start();  // calls server_.start();

 private:
  void onConnection(const muduo::net::TcpConnectionPtr& conn);

  void onMessage(const muduo::net::TcpConnectionPtr& conn,
                 muduo::net::Buffer* buf,
                 muduo::Timestamp time);

  World* world_;
  muduo::net::TcpServer server_;
};


SceneServer::SceneServer(muduo::net::EventLoop* loop,
                       const muduo::net::InetAddress& listenAddr, World* world, int threadNum)
  : server_(loop, listenAddr, "SceneServer"), world_(world)
{
  server_.setConnectionCallback(
      boost::bind(&SceneServer::onConnection, this, _1));
  server_.setMessageCallback(
      boost::bind(&SceneServer::onMessage, this, _1, _2, _3));
  server_.setThreadNum(threadNum);
}

void SceneServer::start()
{
  server_.start();
}

void SceneServer::onConnection(const muduo::net::TcpConnectionPtr& conn)
{
  LOG_INFO << "SceneServer - " << conn->peerAddress().toIpPort() << " -> "
           << conn->localAddress().toIpPort() << " is "
           << (conn->connected() ? "UP" : "DOWN");

  if (conn->connected()) 
  {
      muduo::net::TcpConnectionPtr con(conn);
      g_World->RegisterUser(con);
  } else {
      g_World->DeleteUser(conn);
  }
}

void SceneServer::onMessage(const muduo::net::TcpConnectionPtr& conn,
                           muduo::net::Buffer* buf,
                           muduo::Timestamp time)
{
    while (buf->readableBytes() > sizeof(int32_t)) {
        const int32_t msgLen = buf->checkPeekInt32();
        if ( msgLen > buf->readableBytes() ) {
            LOG_INFO << "MSG LEN : " << buf->peekInt32() << " buf len : " << buf->readableBytes();
            break;
        }
        
        struct PACKET pkt(std::string(conn->name().c_str()));
        pkt.len = buf->checkReadInt32();
        pkt.cmd = buf->checkReadInt32();
        pkt.uid = buf->checkReadInt32();
        //LOG_INFO << "OnMessage msgLen : " << msgLen << " len : " << pkt.len - PACKET_HEAD_LEN;
        pkt.msg = std::string(buf->retrieveAsString(pkt.len - PACKET_HEAD_LEN).c_str(), pkt.len - PACKET_HEAD_LEN);

        switch (pkt.cmd)
        {
            case uint32(cs::ProtoID::ID_C2S_Ping):
            {
                cs::C2S_Ping ping;
                if ( !ping.ParseFromString(pkt.msg) )
                {
                    LOG_ERROR << "Proto ping parse error";
                    return;
                }

                uint32 send_time = ping.send_time();

                struct timeval tv;
                gettimeofday(&tv,NULL);
                uint32 recv_time = tv.tv_sec*1000 + tv.tv_usec/1000;

                cs::S2C_Ping sping;
                sping.set_send_time(send_time);
                sping.set_recv_time(recv_time);

                std::string msg;
                msg = sping.SerializeAsString();
                std::string smsg = g_World->BuildPacket(0, uint32(cs::ProtoID::ID_S2C_Ping), msg);
                conn->send(smsg.c_str(), smsg.size());
                return;
            }   
        }

        g_World->PushMsg(pkt);
    }
}

int main(int argc, char* argv[]) 
{
    LOG_INFO << "pid = " << getpid();
    if (argc < 2) {LOG_INFO << "usage : ./scene <threadNum>"; return 0; }

    g_World = new World();

    muduo::net::EventLoop loop;
    muduo::net::InetAddress listenAddr(2007);
    SceneServer server(&loop, listenAddr, g_World, atoi(argv[1]));

    server.start();
    loop.loop();

    delete g_World;

    google::protobuf::ShutdownProtobufLibrary();
    return 0;
}

