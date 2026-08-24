/* The Sage's Tunic grants passive resistances from owned medallions. */

static void Sages_Behavior(Player* player, PlayState* play) {
    (void)player;
    (void)play;

    ExtEquip_SagesFlashTick();
}
