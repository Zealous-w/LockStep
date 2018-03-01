#ifndef GAME_SESSION_H
#define GAME_SESSION_H
#include <Net.h>
#include <Queue.h>
#include <map>

#include <Player.h>
#include <proto/cs.pb.h>

class LockStepServer;
class ClientSession {
public:
    typedef std::function<bool(struct PACKET&)> ServiceFunc;
    ClientSession(LockStepServer* server, const khaki::TcpClientPtr& conn);
    ~ClientSession();

	void OnMessage(const khaki::TcpClientPtr& con);

    void RegisterCmd();
    void DispatcherCmd(struct PACKET& msg);
    void SendPacket(struct PACKET& pkt);
    void SendPacket(uint32 cmd, uint32 uid, std::string& msg);
    struct PACKET BuildPacket(uint32 cmd, uint32 uid, std::string& msg);
private:
    khaki::TcpClientPtr conn_;
    LockStepServer* server_;
    std::map<uint32, ServiceFunc> command_;
public:
    bool HandlerPing(struct PACKET& pkt);
    bool HandlerDirtyPacket(struct PACKET& pkt);
};

typedef std::shared_ptr<ClientSession> ClientSessionPtr;
#endif