#include <unistd.h>
#include <muduo/base/Logging.h>
#include "World.h"

World::World() : 
    thread_(&World::Loop, this)
{
    uidIndex = 0;
    RegisterCommand();
}
World::~World()
{
    thread_.join();
}

void World::Loop()
{
    running_ = true;
    while(running_)
    {
        struct PACKET tmp = GetMsg();
        DispatherCommand(tmp);
    }
}

void World::Stop()
{
    running_ = false;
}

void World::PushMsg(struct PACKET& msg)
{
    msgQueue.put(msg);
}

struct PACKET World::GetMsg()
{
    return msgQueue.take();
}

PlayerPtr World::GetUser(std::string name)
{
    if ( sessions_.find(name) != sessions_.end())
    {
        return sessions_[name];
    }
    return PlayerPtr();
}

bool World::RegisterUser(muduo::net::TcpConnectionPtr& conn)
{   
    std::string name(conn->name().c_str());
    muduo::MutexLockGuard lck(mutex_);
    assert(sessions_.find(name) == sessions_.end());
    sessions_.insert(std::make_pair(name, PlayerPtr(new Player(conn))));
    LOG_INFO << "AddUser -> name : " << name;
    return true;
}

bool World::DeleteUser(const muduo::net::TcpConnectionPtr& conn)
{
    muduo::MutexLockGuard lck(mutex_);
    std::string name(conn->name().c_str());
    assert(sessions_.find(name) != sessions_.end());
    sessions_.erase(name);
    LOG_INFO << "DeletUser -> name : " << name;
}

 void World::RegisterCommand()
 {
    REGISTER_CMD_CALLBACK(cs::ProtoID::ID_C2S_Login, HandlerLogin);
    REGISTER_CMD_CALLBACK(cs::ProtoID::ID_C2S_Move, HandlerMove);
    REGISTER_CMD_CALLBACK(cs::ProtoID::ID_C2S_Attack, HandlerAttack);
 }

 void World::DispatherCommand(struct PACKET& msg)
 {
    LOG_INFO << "msg.cmd : " << msg.cmd;
    if ( command_.find(msg.cmd) != command_.end() )
    {
        PlayerPtr player = GetUser(std::string(msg.connName));
        if ( player )
        {
            command_[msg.cmd](player, msg.msg);
            return;
        }
        LOG_ERROR << "not found this user : " << msg.uid;
    }
    else
    {
        LOG_ERROR << "error proto : " << msg.cmd;
    }
 }

 std::string World::BuildPacket(uint32 dwUid, uint32 dwCmdId, std::string& msg)
 {
    struct PACKET pkt("name");
    pkt.len = PACKET_HEAD_LEN + msg.size();
    pkt.cmd = uint32(dwCmdId);
    pkt.uid = dwUid;
    pkt.msg = msg;
    
    char* msg_ = new char[pkt.len];
    memset(msg_, 0, pkt.len);

    memcpy(msg_, (char*)&(pkt.len), sizeof(uint32));
    memcpy(msg_ + 4, (char*)&pkt.cmd, sizeof(uint32));
    memcpy(msg_ + 8, (char*)&pkt.uid, sizeof(uint32));
    memcpy(msg_ + 12, pkt.msg.c_str(), msg.size());

    LOG_INFO << "message size : " << *(uint32*)msg_;
    std::string smsg(msg_, pkt.len);
    delete msg_;
    return smsg;
 }

void World::BroadcastPacket(uint32 dwCmdId, std::string& str)
{
    muduo::MutexLockGuard lck(mutex_);
    for (MapConnections::iterator it = sessions_.begin(); 
                it != sessions_.end(); ++it )
    {
        LOG_INFO << "UID : " << it->second->GetUid();
        std::string msg = BuildPacket(it->second->GetUid(), dwCmdId, str);
        it->second->SendPacket(msg);
    }
}

void World::BroadcastPacket(PlayerPtr& play, uint32 dwCmdId, std::string& str)
{
    muduo::MutexLockGuard lck(mutex_);
    for (MapConnections::iterator it = sessions_.begin(); 
                it != sessions_.end(); ++it )
    {
        if ( it->second->GetUid() == play->GetUid() ) continue;
        std::string msg = BuildPacket(it->second->GetUid(), dwCmdId, str);
        it->second->SendPacket(msg);
    }
}

/////////////////////////
bool World::HandlerLogin(PlayerPtr& play, std::string& str)
{
    cs::C2S_Login login;
    if ( !login.ParseFromString(str) )
    {
        LOG_ERROR << "Proto parse error";
        return false;
    }
    //////////////
    if (play->GetUid() != 0) {
        LOG_ERROR << "This player already exits";
        return false;
    }

    uidIndex++;
    play->SetUid(uidIndex);

    cs::S2C_Login slogin;
    slogin.set_uid(uidIndex);
    std::string msg;
    msg = slogin.SerializeAsString();

    LOG_INFO << "HandlerLogin cur uid : " << uidIndex;
    //////////////
    std::string smsg = BuildPacket(play->GetUid(), uint32(cs::ProtoID::ID_S2C_Login), msg);
    play->SendPacket(smsg);
    SendAllPosUsers();
    return true;
}

bool World::HandlerMove(PlayerPtr& play, std::string& str)
{
    cs::C2S_Move move;
    if ( !move.ParseFromString(str) )
    {
        LOG_ERROR << "Proto parse error";
        return false;
    }
    LOG_INFO << "HandlerMove : ";

    uint32 dwx = move.dwx();
    uint32 dwy = move.dwy();
    //////
    play->SetPosX(dwx);
    play->SetPosY(dwy);
    //////
    // cs::S2C_Move smove;
    // smove.set_dwuid(play->GetUid());
    // smove.set_dwx(dwx);
    // smove.set_dwy(dwx);
    // std::string msg;
    // msg = smove.SerializeAsString();

    // 给视野中的所有人广播位置'
    // std::string smsg = BuildPacket(play->GetUid(), uint32(cs::ProtoID::ID_S2C_Move), msg);
    // play->SendPacket(smsg);
    SendAllPosUsers();
    //BroadcastPacket(uint32(cs::ProtoID::ID_S2C_Move), msg);
    return true;
}

bool World::HandlerAttack(PlayerPtr& play, std::string& str)
{
    return true;
}

void World::SendAllPosUsers() {
    cs::S2C_AllPos spos;
    cs::User* us;

    for (MapConnections::iterator it = sessions_.begin(); 
                it != sessions_.end(); ++it )
    {
        us = spos.add_users();
        us->set_dwx(it->second->GetPosX());
        us->set_dwy(it->second->GetPosY());
        us->set_uid(it->second->GetUid());
    }

    std::string msg;
    msg = spos.SerializeAsString();
    BroadcastPacket(uint32(cs::ProtoID::ID_S2C_AllPos), msg);
}