#ifndef COMMON_H
#define COMMON_H
#include <Buffer.h>
#include <arpa/inet.h>
#include <Log.h>

typedef unsigned int uint32;
typedef unsigned short uint16;
typedef unsigned char uint8;
typedef unsigned long long uint64;


const uint32 PACKET_HEAD_LEN = 12;
struct PACKET {
    PACKET(){}
    PACKET(uint32_t uniqueId):id(uniqueId){}
    uint32 len;
    uint32 cmd;
    uint32 uid;
    std::string msg;
    
    uint32_t id;
};

inline std::string Encode(struct PACKET& pkt) {
    char* msg_ = new char[pkt.len];
    char* msg = msg_;
    memset(msg_, 0, pkt.len);
    
    *reinterpret_cast<uint32_t*>(msg_) = htonl(pkt.len);
    msg_ += sizeof(uint32_t);
    *reinterpret_cast<uint32_t*>(msg_) = htonl(pkt.cmd);
    msg_ += sizeof(uint32_t);
    *reinterpret_cast<uint32_t*>(msg_) = htonl(pkt.uid);
    msg_ += sizeof(uint32_t);
    pkt.msg.copy(msg_, pkt.msg.size(), 0);
    std::string smsg(msg, pkt.len);
    delete[] msg;
    return smsg;
}

inline struct PACKET Decode(char* src) {
    struct PACKET pkt;
    //ntohl(*reinterpret_cast<const uint32_t>(src))
    pkt.len = ntohl(*reinterpret_cast<const uint32_t*>(src));
    src += sizeof(uint32_t);
    pkt.cmd = ntohl(*reinterpret_cast<const uint32_t*>(src));
    src += sizeof(uint32_t);
    pkt.uid = ntohl(*reinterpret_cast<const uint32_t*>(src));
    src += sizeof(uint32_t);
    pkt.msg = std::string(src, pkt.len - PACKET_HEAD_LEN);
    return pkt;
}

inline bool checkBufValid(khaki::Buffer& buf) {
    char* src = buf.begin();
    uint32_t len = buf.size();
    uint32_t pktLen = ntohl(*reinterpret_cast<const uint32_t*>(src));
    //log4cppDebug(khaki::logger, "pktLen : %d, len : %d", pktLen, len);
    if ( pktLen > len ) {
        return false;
    }
    return true;
}

#define DEFINE_CLASS(CLASS_NAME) \
    class CLASS_NAME { \
    private: \
        CLASS_NAME(); \
        ~CLASS_NAME(); \
        CLASS_NAME(const CLASS_NAME&) {} \
	    CLASS_NAME& operator=(const CLASS_NAME&) {} \
    public:\
        static CLASS_NAME& GetSystem() {\
            static CLASS_NAME value;\
            return value;\
        }
#define DEFINE_CLASS_END };

#endif
