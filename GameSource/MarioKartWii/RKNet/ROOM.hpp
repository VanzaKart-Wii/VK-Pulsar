#ifndef _RKNETROOM_
#define _RKNETROOM_
#include <kamek.hpp>


namespace RKNet {
enum ROOMRole {
    ROOM_HOST,
    ROOM_GUEST
};

enum ROOMMessageType {
    ROOM_NORMAL_MESSAGE,
    ROOM_START = 0x1,
    ROOM_REGISTER_FRIEND = 0x2,
    ROOM_JOIN_MESSAGE = 0x3,
    ROOM_CHAT_MESSAGE = 0x4
};

#pragma pack(push, 1)
struct ROOMPacket {
    static const u32 idx = 3;
    u8 messageType; //see enum
    u16 message;
    u8 unknown_0x3;
}; //0x4
#pragma pack(pop)
size_assert(ROOMPacket, 0x4);

union ROOMPacketReg {
    u32 packetArg;
    ROOMPacket packet;
};


class ROOMHandler {
public:
    static ROOMHandler* sInstance; //809c20e0
    static ROOMHandler* CreateInstance(); //8065aa88
    static void DestroyInstance(); //8065ab4c
    ROOMHandler(); //8065af84 just a wrapper around Reset
    ~ROOMHandler(); //8065b008

    void Init(ROOMRole role); //8065ab8c
    void Reset(); //8065ac94
    void Update(); //8065ad18
    void SetAllToSendPackets(ROOMPacket packet); //8065ae70  
    ROOMPacket GetRecvPacket(u8 aid) const; //8065af64
    void SetToSendPacket(u8 aid, ROOMPacket packet); //8065af74
    void ExportROOMandUSER(); //8065b048
    void ImportNewPackets(); //8065b194
    void ClearToSendPacket(u8 aidDest); //8065b2ec
    void ClearRecvPacket(u8 aid); //8065b300
    void SetToSendPacke2(u8 aid, ROOMPacket packet); //8065b314
    void SetRecvPacket(u8 aid, ROOMPacket packet); //8065b324
    ROOMPacket* GetToSendPacket(u8 aid) const; //8065b334
    ROOMPacket* GetRecvPacketPtr(u8 aid) const; //8065b344
    void ResetAllPackets(); //8065b260
    void InitPacket(ROOMPacket packet); //8065b2e0

    u8 unknown_0x0[4];
    ROOMRole role; //0x4
    u8 vanillaPackets[0x68 - 0x8]; //0x8 toSend/receivedPackets[12], moved below for 24 aids (Race/Players24/Online24Handlers.inc)
    u8 lastSentToAid; //0x68
    u8 unknown_0x69[0x70 - 0x69];
    u64 lastSentTime;
    u32 unknown_0x78[2];
    ROOMPacket toSendPackets[24]; //0x80
    ROOMPacket receivedPackets[24]; //0xe0
}; //total size 0x140 (0x80 in the game)
size_assert(ROOMHandler, 0x140);
}//namespace RKNet
#endif