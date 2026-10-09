#ifndef _PULSAR_STAFF_BADGE_HPP_
#define _PULSAR_STAFF_BADGE_HPP_

#include <kamek.hpp>

/*
    Badges reserved to the people who worked on VanzaKart.

    They are not ranks: they are not earned, never replace the prestige rank and do not
    count towards it. In the menus a contributor who is also ranked shows both, staff badge
    first: "<staff><rank> Name". In a race only the staff badge shows, on the right of the
    name (RatingUI.cpp, SetRankBMG).

    Nothing is transmitted. Every console resolves the badge itself by matching the
    player's profile ID (the low 32 bits of the friend code, authenticated by the WFC
    server) against the list in StaffBadge.cpp, so a badge cannot be claimed by editing
    a packet.
*/

namespace Pulsar {
namespace PointRating {
namespace Staff {

// Each role is drawn as the glyph Config::STAFF_GLYPH_BASE + role in
// tt_kart_extension_font.brfnt, right after the rank badges.
enum Role {
    ROLE_NONE = 0,
    ROLE_MODERATOR,          // 0xF085
    ROLE_LEADER,             // 0xF086, the VanzaKart logo: the creator
    ROLE_STAFF_GHOST,        // 0xF087
    ROLE_DEVELOPER,          // 0xF088
    ROLE_CREATIVE_DIRECTOR,  // 0xF089
    ROLE_TRANSLATOR,         // 0xF08A
    ROLE_BETA_TESTER,        // 0xF08B
    ROLE_COUNT
};

Role GetForProfileId(u32 profileId);

// Only the console's first player carries the badge: the second one is a guest on the
// same licence, not the contributor.
Role GetLocal(u8 playerIndexOnConsole);
Role GetRemote(u8 aid, u8 playerIndexOnConsole);

// Resolves any player of the current race, local or remote.
Role GetForPlayer(u8 playerId);

// Glyph for a role, or 0 when the role draws nothing.
wchar_t GetBadgeGlyph(Role role);

/*
    The rank icon index to draw for a console slot, given the one that console sent.

    The icon index rides the sender's starRank (RatingHooks), so on its own the badge in
    the race and the results is whatever the sender's build decided: a console on a build
    that predates a role, or that hand-edits the packet, would show the wrong one. Here the
    staff part is resolved from the list on this console like everywhere else, and only
    the prestige rank is taken from the sender.
*/
u8 ResolveRankIconIndex(u8 aid, u8 playerIndexOnConsole, u8 sentIndex);

// The message that draws a role's badge in a race (OnlineParams::rankBMG, the handle_text panes on the right of
// the name), or 0 when the role draws nothing.
u32 GetIconBMG(Role role);

}  // namespace Staff
}  // namespace PointRating
}  // namespace Pulsar

#endif  // _PULSAR_STAFF_BADGE_HPP_
