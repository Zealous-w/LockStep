#include <Net.h>
#include <Log.h>
#include <Queue.h>
#include <map>
#include <memory>
#include <set>
#include <thread>
#include <sys/time.h>
#include "LockStepSession.h"
#include "World.h"
#include "Player.h"
#include "Global.h"
///////////////////////////////

///////////////////////////////
class LockStepServer {
public: 
    LockStepServer(khaki::EventLoop* loop, std::string host, int port, int threadNum);
    ~LockStepServer();

    void start();

    void OnConnection(const khaki::TcpClientPtr& con);
	void OnConnClose(const khaki::TcpClientPtr& con);
private:
    khaki::TcpThreadServer server_;
	std::mutex mtx_;
    std::map<uint32, ClientSessionPtr> sessionLists_;
};

LockStepServer::LockStepServer(khaki::EventLoop* loop, std::string host, int port, int threadNum) :
        server_(loop, host, port, threadNum) {
    server_.setConnectionCallback(std::bind(&LockStepServer::OnConnection, 
                        this, std::placeholders::_1));
    server_.setConnCloseCallback(std::bind(&LockStepServer::OnConnClose, 
                        this, std::placeholders::_1));
}

LockStepServer::~LockStepServer(){}

void LockStepServer::start() {
    server_.start();
}

void LockStepServer::OnConnection(const khaki::TcpClientPtr& con) {
    g_World.RegisterUser(con);
    std::unique_lock<std::mutex> lck(mtx_);
    sessionLists_[con->getUniqueId()] = ClientSessionPtr(new ClientSession(this, con));
    log4cppDebug(khaki::logger, "REGISTER fd : %d add sessionlists", con->getUniqueId());
}
void LockStepServer::OnConnClose(const khaki::TcpClientPtr& con) {
    g_World.DeleteUser(con);
    sessionLists_.erase(con->getUniqueId());
    log4cppDebug(khaki::logger, "CLOSE fd : %d closed", con->getUniqueId());
}

int main(int argc, char* argv[]) 
{
    khaki::InitLog(khaki::logger, "./lockstep.log", log4cpp::Priority::DEBUG);
    log4cppDebug(khaki::logger, "pid = %d", getpid());
    if (argc < 2) {
        log4cppDebug(khaki::logger, "usage : ./lockstep <threadNum>"); 
        return 0; 
    }
    g_World.Start();
    khaki::EventLoop loop;
    LockStepServer server(&loop, "192.168.50.10", 2007, atoi(argv[1]));

    server.start();
    loop.loop();

    google::protobuf::ShutdownProtobufLibrary();
    return 0;
}

