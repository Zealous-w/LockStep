#ifndef MUDUO_EXAMPLES_WORLD_H
#define MUDUO_EXAMPLES_WORLD_H

#include <khaki.h>
#include <map>
#include <functional>
#include <thread>
#include <mutex>
#include <queue>
#include <condition_variable>
#include <atomic>

#include "common.h"
#include "Player.h"
#include "proto/cs.pb.h"

#define REGISTER_CMD_CALLBACK(cmdId, func) \
    command_[uint32(cmdId)]  = std::bind(&World::func, this, std::placeholders::_1, std::placeholders::_2)

DEFINE_CLASS(World)
public:
    enum {
        E_GAME_STATUS_READY     = 1,
        E_GAME_STATUS_RUNNING   = 2,
        E_GAME_STATUS_END       = 3,
    };
public:
    typedef std::map<uint32_t, PlayerPtr> MapConnections;
    typedef std::function<bool(PlayerPtr&, std::string&)> ServiceFunc;
    typedef std::map<uint32/*uid*/, std::vector<uint32>/*keyinfo*/> MapKeyInfoRecord;
    void Loop();
    void Start() { std::unique_lock<std::mutex> lck(mtxcond_); running_.store(true); mCond_.notify_all();}
    PlayerPtr GetUser(uint32_t id);
    bool RegisterUser(const khaki::TcpClientPtr& conn);
    bool DeleteUser(const khaki::TcpClientPtr& conn);
    uint32 GetUsersSize() { std::unique_lock<std::mutex> lck(mtxSession_); return sessions_.size(); }

    void CleanData();

    void RegisterCommand();
    void DispatherCommand(struct PACKET& msg);
    void Stop();

    void PushMsg(struct PACKET& msg);
    void ConsumeMsg();
    void MsgProcess(khaki::queue<struct PACKET>& msg);
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
    std::atomic<bool> running_;
    uint32 gameStatus;
    uint32 readyNum;
    int uidIndex;

    std::mutex mtxcond_;
    std::condition_variable mCond_;
    khaki::EventLoop loop_;

    khaki::queue<struct PACKET> msgQueue_;
    std::thread thread_;
    std::map<uint32, ServiceFunc> command_;
    MapConnections sessions_;
    std::mutex mtxSession_;

    uint32 upStep;
    uint64 curFrameId;
    uint64 nextFrameId;
    MapKeyInfoRecord vCurFrameInfo; //当前帧指令
    std::map<uint32/*frameId*/, MapKeyInfoRecord> mOldFrameInfo; //历史帧指令
public:
    bool HandlerLogin(PlayerPtr& play, std::string& str);
    bool HandlerMove(PlayerPtr& play, std::string& str);
    bool HandlerAttack(PlayerPtr& play, std::string& str);
    bool HandlerReady(PlayerPtr& play, std::string& str);
    bool HandlerGetFrameInfo(PlayerPtr& play, std::string& str);
    bool HandlerExit(PlayerPtr& play, std::string& str);
    bool HandlerGameOver(PlayerPtr& play, std::string& str);
    
DEFINE_CLASS_END

#define g_World World::GetSystem()
#endif