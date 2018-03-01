#include <LockStepSession.h>
#include <Global.h>

ClientSession::ClientSession(LockStepServer* server, const khaki::TcpClientPtr& conn) :
        server_(server), conn_(conn) {
    conn_->setReadCallback(std::bind(&ClientSession::OnMessage, 
                        this, std::placeholders::_1));
    RegisterCmd();
}
ClientSession::~ClientSession(){}

void ClientSession::OnMessage(const khaki::TcpClientPtr& con) {
    khaki::Buffer& buf = con->getReadBuf();
    //log4cppDebug(khaki::logger, "meesgae %d", buf.size());
    while( buf.size() > 0 ) {
        if (!checkBufValid(buf)) {
            //log4cppDebug(khaki::logger, "meesgae %d", buf.size());
            break;
        }
        struct PACKET pkt = Decode(buf.begin());
        pkt.id = con->getUniqueId();
        buf.addBegin(pkt.len);
        DispatcherCmd(pkt);
    }
}

void ClientSession::RegisterCmd() {
    command_[cs::ProtoID::ID_C2S_Ping] = std::bind(&ClientSession::HandlerPing, this, std::placeholders::_1);
}

void ClientSession::DispatcherCmd(struct PACKET& msg) {
    if ( command_.find(msg.cmd) != command_.end() ) {
        command_[msg.cmd](msg);
    } else {
        HandlerDirtyPacket(msg);
        //log4cppDebug(khaki::logger, "error proto : %d", msg.cmd);
    }
}

void ClientSession::SendPacket(uint32 cmd, uint32 uid,std::string& msg) {
    struct PACKET pkt;
    pkt.len = PACKET_HEAD_LEN + msg.size();
    pkt.cmd = cmd;
    pkt.uid = uid;
    pkt.msg = msg;
    SendPacket(pkt);
}

struct PACKET ClientSession::BuildPacket(uint32 cmd, uint32 uid, std::string& msg) {
    struct PACKET pkt;
    pkt.len = PACKET_HEAD_LEN + msg.size();
    pkt.cmd = cmd;
    pkt.uid = uid;
    pkt.msg = msg;
    return pkt;
}

void ClientSession::SendPacket(struct PACKET& pkt) {
    std::string msg = Encode(pkt);
    conn_->send(msg.data(), msg.size());
}

bool ClientSession::HandlerPing(struct PACKET& pkt) {
    cs::C2S_Ping ping;
    if ( !ping.ParseFromString(pkt.msg) )
    {
        //LOG_ERROR << "Proto ping parse error";
        return false;
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
    struct PACKET csPkt = this->BuildPacket(uint32(cs::ProtoID::ID_S2C_Ping), 0, msg);
    this->SendPacket(csPkt);
    //conn->send(smsg.c_str(), smsg.size());
    //log4cppDebug(khaki::logger, "HandlerPing: %d", recv_time);
    return true;
}

bool ClientSession::HandlerDirtyPacket(struct PACKET& pkt) {
    g_World.PushMsg(pkt);
    //log4cppDebug(khaki::logger, "HandlerDirtyPacket: %d", pkt.cmd);
    return false;
}