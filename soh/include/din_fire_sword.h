#ifndef DIN_FIRE_SWORD_H
#define DIN_FIRE_SWORD_H

#include <stdint.h>

struct PlayState;
struct Player;

#ifdef __cplusplus
extern "C" {
#endif

uint32_t DinFireSword_DamageFlags(struct PlayState* play, struct Player* player, uint32_t original);
uint32_t DinFireSword_SetDamageFlags(struct PlayState* play, struct Player* player, int quad, uint32_t original);
void DinFireSword_RefreshDamage(struct PlayState* play, struct Player* player);
void DinFireSword_Reset(void);
void DinFireSword_Update(struct PlayState* play, struct Player* player);
// Gameplay left-hand matrix must be current. Ordinary sword hand only;
// other weapon owners, PAK overrides and ceremonies keep their own draw path.
void DinFireSword_Draw(struct PlayState* play, struct Player* player);

#ifdef __cplusplus
}
#endif
#endif
