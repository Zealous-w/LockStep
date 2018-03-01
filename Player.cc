#include "Player.h"

Player::Player(const khaki::TcpClientPtr& conn) : conn_(conn)
{
    dwUid = 0;
    dwX = 10;
    dwY = 10;
}

Player::~Player() 
{
    log4cppDebug(khaki::logger, "~Player() ->　uid : %d", dwUid);
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