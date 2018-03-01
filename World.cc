#include <unistd.h>
#include "World.h"

World::World() : 
    thread_(&World::Loop, this)
{
    uidIndex = 0;
    readyNum = 0;
    curFrameId = 0;
    nextFrameId = 0;
    gameStatus = E_GAME_STATUS_READY;
    running_ = false;
    RegisterCommand();
}
World::~World()
{
    thread_.join();
}

void World::Loop()
{
    {
        std::unique_lock<std::mutex> lck(mtxcond_);
        mCond_.wait(lck, [this]()->bool{ return running_; });
    }
    while(running_)
    {
        loop_.getPoll()->poll(100);
        MsgProcess(msgQueue_);
        if ( gameStatus == E_GAME_STATUS_RUNNING )
        {
            LogicFrameRefresh();
        }
    }
}

void World::Stop()
{
    running_ = false;
}

void World::PushMsg(struct PACKET& msg)
{
    // std::unique_lock<std::mutex> lck(mtx_);
    // m_msg_queue.push(msg);
    // lck.unlock();
    // cond_.notify_all();
    msgQueue_.push(msg);
}

void World::MsgProcess(khaki::queue<struct PACKET>& msg) {
    if ( msg.size() > 0 ) {
        std::queue<struct PACKET> tmpQueue = msg.popAll();
        while ( !tmpQueue.empty() ) {
            struct PACKET pkt = tmpQueue.front();
            DispatherCommand(pkt);
            tmpQueue.pop();
        }
    }
}

void World::ConsumeMsg()
{
    // std::unique_lock<std::mutex> lck(mtx_);
	// while(m_msg_queue.empty()){
	// 	cond_.wait(lck);
	// }

    // m_tmp_queue.swap(m_msg_queue);
    // lck.unlock();
	
    // while(!m_tmp_queue.empty()) {
	// 	struct PACKET pkt = m_tmp_queue.front();
	// 	m_tmp_queue.pop();
    //     DispatherCommand(pkt);
	// }
}

bool World::MsgEmpty()
{
    // std::unique_lock<std::mutex> lck(mtx_);
    // return m_msg_queue.empty();
}

PlayerPtr World::GetUser(uint32_t id)
{
    if ( sessions_.find(id) != sessions_.end())
    {
        return sessions_[id];
    }
    return PlayerPtr();
}

void World::CleanData() {
    uidIndex = 0;
    readyNum = 0;
    gameStatus = E_GAME_STATUS_READY;
}

bool World::RegisterUser(const khaki::TcpClientPtr& conn)
{   
    std::unique_lock<std::mutex> lck(mtxSession_);
    assert(sessions_.find(conn->getUniqueId()) == sessions_.end());
    sessions_.insert(std::make_pair(conn->getUniqueId(), PlayerPtr(new Player(conn))));
    log4cppDebug(khaki::logger, "AddUser -> ID : %d", conn->getUniqueId());
    return true;
}

bool World::DeleteUser(const khaki::TcpClientPtr& conn)
{
    std::unique_lock<std::mutex> lck(mtxSession_);
    assert(sessions_.find(conn->getUniqueId()) != sessions_.end());
    sessions_.erase(conn->getUniqueId());
    
    gameStatus = E_GAME_STATUS_READY;
    if (readyNum > 0) {
        readyNum--;
    }
    if (readyNum == 0) {
        this->CleanData();
    }
    log4cppDebug(khaki::logger, "readyNum=%u, status=%u", readyNum, gameStatus);
}

 void World::RegisterCommand()
 {
    REGISTER_CMD_CALLBACK(cs::ProtoID::ID_C2S_Login, HandlerLogin);
    REGISTER_CMD_CALLBACK(cs::ProtoID::ID_C2S_Move, HandlerMove);
    REGISTER_CMD_CALLBACK(cs::ProtoID::ID_C2S_Attack, HandlerAttack);
    REGISTER_CMD_CALLBACK(cs::ProtoID::ID_C2S_Ready, HandlerReady);
    REGISTER_CMD_CALLBACK(cs::ProtoID::ID_C2S_Frame, HandlerGetFrameInfo);
    REGISTER_CMD_CALLBACK(cs::ProtoID::ID_C2S_Exit, HandlerExit);
    REGISTER_CMD_CALLBACK(cs::ProtoID::ID_C2S_GameOver, HandlerGameOver);
 }

 void World::DispatherCommand(struct PACKET& msg)
 {
    if ( command_.find(msg.cmd) != command_.end() )
    {
        PlayerPtr player = GetUser(msg.id);
        if ( player )
        {
            command_[msg.cmd](player, msg.msg);
            return;
        }
        log4cppDebug(khaki::logger, "not found this user : %d %d", msg.uid, msg.cmd);
    }
    else
    {
        log4cppDebug(khaki::logger, "error proto : %d", msg.cmd);
    }
 }

 std::string World::BuildPacket(uint32 dwUid, uint32 dwCmdId, std::string& msg)
 {
    struct PACKET pkt;
    pkt.len = PACKET_HEAD_LEN + msg.size();
    pkt.cmd = uint32(dwCmdId);
    pkt.uid = dwUid;
    pkt.msg = msg;
    std::string smsg = Encode(pkt);
    return smsg;
 }

void World::BroadcastPacket(uint32 dwCmdId, std::string& str)
{
    std::unique_lock<std::mutex> lck(mtxSession_);
    for (MapConnections::iterator it = sessions_.begin(); 
                it != sessions_.end(); ++it )
    {
        std::string msg = BuildPacket(it->second->GetUid(), dwCmdId, str);
        it->second->SendPacket(msg);
    }
}

void World::BroadcastPacket(PlayerPtr& play, uint32 dwCmdId, std::string& str)
{
    std::unique_lock<std::mutex> lck(mtxSession_);
    for (MapConnections::iterator it = sessions_.begin(); 
                it != sessions_.end(); ++it )
    {
        if ( it->second->GetUid() == play->GetUid() ) continue;
        std::string msg = BuildPacket(it->second->GetUid(), dwCmdId, str);
        it->second->SendPacket(msg);
    }
}

void World::FrameInitToClient(PlayerPtr& play)
{
    cs::S2C_FrameInit frame;

    frame.set_cur_frame_id(curFrameId);
    frame.set_next_frame_id(nextFrameId);

    std::string msg;
    msg = frame.SerializeAsString();
    
    std::string smsg = BuildPacket(play->GetUid(), uint32(cs::ProtoID::ID_S2C_FrameInit), msg);
    play->SendPacket(smsg);
}

void World::LogicFrameRefresh()
{
    cs::S2C_Frame sframe;
    cs::UserFrame* uframe;
    
    curFrameId = nextFrameId;
    nextFrameId++;

    sframe.set_frame_id(curFrameId);
    sframe.set_nextframe_id(nextFrameId);

    for (auto item : vCurFrameInfo)
    {
        uframe = sframe.add_users();
        uframe->set_uid(item.first);
        for ( auto v : item.second )
        {
            uframe->add_key_info(v);
        }
    }
    mOldFrameInfo[curFrameId] = vCurFrameInfo;
    //log4cppDebug(khaki::logger, "LogicFrameRefresh %u, %u, %u", vCurFrameInfo.size(), sframe.frame_id(), sframe.nextframe_id());
    vCurFrameInfo.clear();
    std::string msg;
    msg = sframe.SerializeAsString();
    BroadcastPacket(uint32(cs::ProtoID::ID_S2C_Frame), msg);
}

void World::SendUserLoginInfo()
{
    cs::S2C_Sight sight;
    cs::User* user;
    for (auto player : sessions_)
    {
        user = sight.add_users();
        user->set_uid(player.second->GetUid());
        user->set_dwx(player.second->GetPosX());
        user->set_dwy(player.second->GetPosY());
    }

    std::string msg;
    msg = sight.SerializeAsString();
    BroadcastPacket(uint32(cs::ProtoID::ID_S2C_Sight), msg);
}

/////////////////////////
bool World::HandlerLogin(PlayerPtr& play, std::string& str)
{
    cs::C2S_Login login;
    if ( !login.ParseFromString(str) )
    {
        //LOG_ERROR << "Proto parse error";
        log4cppDebug(khaki::logger, "Proto parse error");
        return false;
    }
    //////////////
    if (play->GetUid() != 0) {
        //LOG_ERROR << "This player already exits";
        log4cppDebug(khaki::logger, "This player already exits %u", play->GetUid());
        return false;
    }

    uidIndex++;
    play->SetUid(uidIndex);

    cs::S2C_Login slogin;
    slogin.set_uid(uidIndex);
    std::string msg;
    msg = slogin.SerializeAsString();

    log4cppDebug(khaki::logger, "HandlerLogin cur uid : %d", uidIndex);
    //////////////
    std::string smsg = BuildPacket(play->GetUid(), uint32(cs::ProtoID::ID_S2C_Login), msg);
    play->SendPacket(smsg);

    FrameInitToClient(play);
    SendUserLoginInfo();
    //SendAllPosUsers();
    
    return true;
}

bool World::HandlerMove(PlayerPtr& play, std::string& str)
{
    cs::C2S_Move move;
    if ( !move.ParseFromString(str) )
    {
        //LOG_ERROR << "Proto parse error";
        return false;
    }
    //LOG_INFO << "HandlerMove : ";

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
    //SendAllPosUsers();
    //BroadcastPacket(uint32(cs::ProtoID::ID_S2C_Move), msg);
    return true;
}

bool World::HandlerAttack(PlayerPtr& play, std::string& str)
{
    return true;
}

bool World::HandlerReady(PlayerPtr& play, std::string& str)
{
    cs::C2S_Ready ready;
    if ( !ready.ParseFromString(str) )
    {
        log4cppDebug(khaki::logger, "Proto parse error");
        return false;
    }
   //LOG_INFO << "Handler Ready";
    if (play->GetUid() != 0)
    {
        readyNum++;
        if ( readyNum == this->GetUsersSize() ) 
        {
            gameStatus = E_GAME_STATUS_RUNNING;
        }
    }
    else 
        log4cppDebug(khaki::logger, "Invalid uid. Error");

    log4cppDebug(khaki::logger, "HandlerReady");
    return true;
}

bool World::HandlerGetFrameInfo(PlayerPtr& play, std::string& str)
{
    cs::C2S_Frame frame;
    if ( !frame.ParseFromString(str) )
    {
        //LOG_ERROR << "Proto parse error";
        return false;
    }
    //LOG_INFO << "frame info";
    uint32 uid = frame.uid();
    uint64 frameId = frame.frameid();

    int size = frame.key_info_size();
    for (int i = 0; i < size; i++)
    {
        uint32 key = frame.key_info(i);
        vCurFrameInfo.insert(std::make_pair(uid, std::vector<uint32>())).first->second.push_back(key);
    }
}

bool World::HandlerExit(PlayerPtr& play, std::string& str) {
    cs::C2S_Exit recv;
    if ( !recv.ParseFromString(str) )
    {
        log4cppDebug(khaki::logger, "Proto parse error");
        return false;
    }
}

bool World::HandlerGameOver(PlayerPtr& play, std::string& str) {
    cs::C2S_GameOver recv;
    if ( !recv.ParseFromString(str) )
    {
        log4cppDebug(khaki::logger, "Proto parse error");
        return false;
    }

    //do something
    gameStatus = E_GAME_STATUS_END;
    this->CleanData();
    curFrameId = 0;
    nextFrameId = 0;
    mOldFrameInfo.clear();
    ///////
    cs::S2C_GameOver csMsg;
    csMsg.set_ret(1);
    csMsg.set_uid(recv.uid());
    std::string msg;
    msg = csMsg.SerializeAsString();
    //////////////
    std::string smsg = BuildPacket(play->GetUid(), uint32(cs::ProtoID::ID_S2C_GameOver), msg);
    play->SendPacket(smsg);
}

void World::SendAllPosUsers() 
{
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

void World::SendAllKeyInfo()
{
}