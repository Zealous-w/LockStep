#ifndef MUDUO_EXAMPLES_SCNEE_H
#define MUDUO_EXAMPLES_SCNEE_H

#include <muduo/net/TcpConnection.h>
#include <muduo/base/Logging.h>
#include <memory>
#include <string>

typedef unsigned int uint32;
typedef unsigned long long uint64;

#define DEF_FUNC_PARAM(func, type, var) \
    private:\
        type var;\
    public:\
        void Set##func(type tmp) {\
            var = tmp;\
        }\
        type Get##func() {\
            return var;\
        }

const uint32 PACKET_HEAD_LEN = 12;
struct PACKET {
    PACKET(const std::string name):connName(name){}
    uint32 len;
    uint32 cmd;
    uint32 uid;
    std::string msg;
    
    std::string connName;
};

class Player {
public:
    Player(const muduo::net::TcpConnectionPtr& conn);
    ~Player();

    uint32 GetFd();
    /////Send Data////
    void SendPacket(std::string& str);
    /////Use Skill////
    void UseSkill(uint32 dwSkillId, uint32 dwTargetId);

private:
    muduo::net::TcpConnectionPtr conn_;
    
    DEF_FUNC_PARAM(Uid, uint32, dwUid)
    DEF_FUNC_PARAM(PosX, int, dwX)
    DEF_FUNC_PARAM(PosY, int, dwY)
    DEF_FUNC_PARAM(Hp, uint32, dwHp)
    DEF_FUNC_PARAM(Exp, uint32, dwExp)
    DEF_FUNC_PARAM(Name, std::string, strName)
};
typedef std::shared_ptr<Player> PlayerPtr;
#endif