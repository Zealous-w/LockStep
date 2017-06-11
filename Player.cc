#include "Player.h"

Player::Player(const muduo::net::TcpConnectionPtr& conn) : conn_(conn)
{
    dwX = 10;
    dwY = 10;
}

Player::~Player() 
{
    LOG_INFO << "~Player() ->　uid : " << dwUid;
}

/////Send Data////
void Player::SendPacket(std::string& str)
{
    conn_->send((char*)str.c_str(), str.size());
}
/////Use Skill////
void Player::UseSkill(uint32 dwSkillId, uint32 dwTargetId)
{

}