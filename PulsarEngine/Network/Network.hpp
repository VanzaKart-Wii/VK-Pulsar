#ifndef _PUL_NETWORK_
#define _PUL_NETWORK_

#include <Config.hpp>

namespace Pulsar {
namespace Network {

enum DenyType {
    DENY_TYPE_NORMAL,
    DENY_TYPE_BAD_PACK,
    DENY_TYPE_OTT,
    DENY_TYPE_KICK,
};


static const u32 MAX_TRACK_BLOCKING = 12;

inline bool IsGroupedTrack(PulsarId id) {
    if(id < PULSARID_FIRSTCT || id == PULSARID_NONE) return false;
    switch(id - PULSARID_FIRSTCT) {
        default: return false; //no group defined for this pack
    }
}

/*
    Tracks that are never played online: always out of the worldwides and mogis, out of friend
    rooms through the host's track pool setting. Written the way the Pack Creator lists them: the
    custom cup number (1 = first custom cup, vanilla cups not counted) and the track in it, 1 to 4.
    They stay in their cup offline and are hidden from it online. Don't ban all four tracks of a cup.
*/
#define ONLINE_BANNED_TRACK(cup, track) (((cup) - 1) * 4 + (track) - 1)
inline bool IsOnlineBannedTrack(PulsarId id) {
    if(id < PULSARID_FIRSTCT || id == PULSARID_NONE) return false;
    switch(id - PULSARID_FIRSTCT) {
        //case ONLINE_BANNED_TRACK(12, 3): //track name, so the list stays readable
        //    return true;
        default: return false;
    }
}

/*
    Single variants never played online, same rules as IsOnlineBannedTrack. The track is the number
    of its file in Tracks and the variant the number after the underscore: 215_1.szs is 215, 1.
    The main track (variant 0) can't be banned here, use IsOnlineBannedTrack for that.
*/
inline bool IsOnlineBannedVariant(PulsarId id, u32 variantIdx) {
    if(id < PULSARID_FIRSTCT || id == PULSARID_NONE || variantIdx == 0) return false;
    const u32 track = id - PULSARID_FIRSTCT;
    if(track == 215 && variantIdx == 1) return true; //215_1.szs
    return false;
}
#ifdef BETA
#  define VK_REGION_OFFSET 100
#else
#  define VK_REGION_OFFSET 0
#endif

enum ModeRegion {
    REGION_VS = 104 + VK_REGION_OFFSET,        // plain VS worldwide, same value as the pack's base region
    REGION_OTT = 105 + VK_REGION_OFFSET,       // Online TT worldwide
    REGION_200CC = 112 + VK_REGION_OFFSET,     // 200cc worldwide
    REGION_ITEMRAIN = 113 + VK_REGION_OFFSET,  // Item Rain worldwide
};

extern u32 REGIONID;

// True for the regions that carry a mode of their own, i.e. everything but plain VS.
// These must survive a section load: they are the only record of which mode was picked.
inline bool IsModeRegion(u32 region) {
    return region == REGION_OTT || region == REGION_200CC || region == REGION_ITEMRAIN;
}

class Mgr { //Manages network related stuff within Pulsar
public:
    Mgr() : racesPerGP(3), curBlockingArrayIdx(0), lastGroupedTrackPlayed(false) {}
    u32 hostContext;
    DenyType denyType;
    u8 deniesCount;
    u8 ownStatusData;
    u8 statusDatas[30];
    u8 curBlockingArrayIdx;
    u8 racesPerGP;
    bool lastGroupedTrackPlayed;
    u8 padding[1];
    PulsarId* lastTracks;
};

}//namespace Network
}//namespace Pulsar

#endif