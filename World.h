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
    enum {
        E_GAME_STATUS_READY     = 1,
        E_GAME_STATUS_RUNNING   = 2,
        E_GAME_STATUS_END       = 3,
    };
public:
    typedef std::map<std::string, PlayerPtr> MapConnections;
    typedef std::function<bool(PlayerPtr&, std::string&)> ServiceFunc;
    typedef std::map<uint32/*uid*/, std::vector<uint32>/*keyinfo*/> MapKeyInfoRecord;
    void Loop();
    PlayerPtr GetUser(std::string name);
    bool RegisterUser(muduo::net::TcpConnectionPtr& conn);
    bool DeleteUser(const muduo::net::TcpConnectionPtr& conn);
    uint32 GetUsersSize() { muduo::MutexLockGuard lck(mutex_); return sessions_.size(); }


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

    //下发 玩家登陆信息
    void SendUserLoginInfo();
    //优化：根据视野范围进行广播
    void SendAllPosUsers();
    //下发所有的操作
    void SendAllKeyInfo();
    //帧初始化
    void FrameInitToClient(PlayerPtr& play);
    //逻辑帧下发
    void LogicFrameRefresh();
private:
    bool running_;
    uint32 gameStatus;
    uint32 readyNum;
    int uidIndex;

    muduo::BlockingQueue<struct PACKET> msgQueue;
    std::thread thread_;
    std::map<uint32, ServiceFunc> command_;
    MapConnections sessions_;
    mutable muduo::MutexLock mutex_;

    /////queue
    std::mutex mtx_;
    std::condition_variable cond_;
    std::queue<struct PACKET> m_msg_queue;
    std::queue<struct PACKET> m_tmp_queue;

    uint32 upStep;
    uint64 curFrameId;
    uint64 nextFrameId;
    MapKeyInfoRecord vCurFrameInfo;
public:
    bool HandlerLogin(PlayerPtr& play, std::string& str);
    bool HandlerMove(PlayerPtr& play, std::string& str);
    bool HandlerAttack(PlayerPtr& play, std::string& str);
    bool HandlerReady(PlayerPtr& play, std::string& str);
    bool HandlerGetFrameInfo(PlayerPtr& play, std::string& str);
};
#endif