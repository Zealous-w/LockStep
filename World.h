#ifndef MUDUO_EXAMPLES_WORLD_H
#define MUDUO_EXAMPLES_WORLD_H

#include <muduo/net/TcpConnection.h>
#include <muduo/base/BlockingQueue.h>
#include <muduo/base/Mutex.h>
#include <map>
#include <functional>
#include <thread>
#include <mutex>
#include <queue>
#include <condition_variable>

#include "Player.h"
#include "proto/cs.pb.h"

#define REGISTER_CMD_CALLBACK(cmdId, func) \
    command_[uint32(cmdId)]  = std::bind(&World::func, this, std::placeholders::_1, std::placeholders::_2)

class World {
public:
    World();
    ~World();
public:
    typedef std::map<std::string, PlayerPtr> MapConnections;
    typedef std::function<bool(PlayerPtr&, std::string&)> ServiceFunc;
    void Loop();
    PlayerPtr GetUser(std::string name);
    bool RegisterUser(muduo::net::TcpConnectionPtr& conn);
    bool DeleteUser(const muduo::net::TcpConnectionPtr& conn);

    void RegisterCommand();
    void DispatherCommand(struct PACKET& msg);
    void Stop();

    void PushMsg(struct PACKET& msg);
    void ConsumeMsg();
    bool MsgEmpty();

    /////Send Packet/////
    std::string BuildPacket(uint32 dwUid, uint32 dwCmdId, std::string& msg);
    void BroadcastPacket(uint32 dwCmdId, std::string& str);
    void BroadcastPacket(PlayerPtr& play, uint32 dwCmdId, std::string& str);

    //优化：根据视野范围进行广播
    void SendAllPosUsers();
    //帧初始化
    void FrameInitToClient(PlayerPtr& play);
private:
    bool running_;
    int uidIndex;

    muduo::BlockingQueue<struct PACKET> msgQueue;
    std::thread thread_;
    std::map<uint32, PlayerPtr> users_;
    std::map<uint32, ServiceFunc> command_;
    MapConnections sessions_;
    mutable muduo::MutexLock mutex_;

    /////queue
    std::mutex mtx_;
    std::condition_variable cond_;
    std::queue<struct PACKET> m_msg_queue;
    std::queue<struct PACKET> m_tmp_queue;

    ///客户端是否有操作, 0代表没有
    std::vector<uint32> vPlayerOp;

    uint32 upStep;
    uint64 sumFrameAdd;
    uint64 curFrameId;
    uint64 nextFrameId;
    //std::map<uint64, uint32> mFrameInfo;
    std::vector<uint32> vCurFrameInfo;
public:
    bool HandlerLogin(PlayerPtr& play, std::string& str);
    bool HandlerMove(PlayerPtr& play, std::string& str);
    bool HandlerAttack(PlayerPtr& play, std::string& str);
};
#endif