#ifndef _PUL_GHOSTTAGS_
#define _PUL_GHOSTTAGS_
#include <kamek.hpp>
#include <MarioKartWii/UI/Ctrl/CtrlRace/CtrlRaceBalloon.hpp>

namespace Pulsar {
namespace UI {

void ApplyGhostTagContent(CtrlRaceNameBalloon& balloon, u8 playerId);
bool AreGhostTagsHidden();

}//namespace UI
}//namespace Pulsar
#endif
