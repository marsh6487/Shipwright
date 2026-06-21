/**
 * extended_player.c - Extended player item action system
 *
 * Maps custom ITEM_xxx values to PLAYER_IA_xxx actions, and each custom
 * PLAYER_IA_xxx to its model group / update func / init func.
 *
 * MM-PORT BOUNDARY: every custom item is one row in sNeiItems[] below. To port
 * an item to MM (2ship), copy its logic module + its single descriptor row.
 * The four ExtPlayer_* getters are thin lookups over that table with a vanilla
 * fallback, so there is exactly one place that describes an item's engine glue.
 *
 * Items whose action is a *vanilla* PLAYER_IA (bow combos, swords, medallions ->
 * spells, Chateau Romani -> blue potion) or is chosen dynamically (SW97 arrows ->
 * bow/slingshot by age) are NOT table rows: they alias vanilla behavior and are
 * resolved in ExtPlayer_GetItemAction before the table lookup.
 */

#include "extended_player.h"
#include "extended_inventory.h" // SLOT_*, AGE_REQ_*, NeiItem (Skijer's NEI)
#include "z64.h"
#include "mods/items/custom_items.h"
#include "assets/soh_assets.h" // icon textures (Skijer's NEI)
#include "soh/Enhancements/randomizer/randomizerTypes.h" // RG_* (Skijer's NEI)
#include "soh/Enhancements/randomizer/draw.h"            // Randomizer_Draw* (Skijer's NEI)
#include <stddef.h> // NULL

// External reference to vanilla arrays
extern int8_t sItemActions[];
extern uint8_t sActionModelGroups[];
extern s32 (*sItemActionUpdateFuncs[])(Player* this, PlayState* play);
extern void (*sItemActionInitFuncs[])(PlayState* play, Player* this);

// External vanilla functions used by custom items
extern s32 func_8083485C(Player* this, PlayState* play);
extern s32 Player_UpperAction_Sword(Player* this, PlayState* play);
extern void Player_InitDefaultIA(PlayState* play, Player* this);

// External custom item upper action functions
extern s32 Player_UpperAction_Beetle(Player* this, PlayState* play);
extern s32 Player_UpperAction_BombArrows(Player* this, PlayState* play);
extern s32 Player_UpperAction_CaneOfSomaria(Player* this, PlayState* play);
extern s32 Player_UpperAction_DekuLeaf(Player* this, PlayState* play);
extern s32 Player_UpperAction_Shovel(Player* this, PlayState* play);
extern s32 Player_UpperAction_SwitchHook(Player* this, PlayState* play);

// External custom item init functions (not declared in custom_items.h)
extern void Player_InitHyliasGraceIA(PlayState* play, Player* this);
extern void Player_InitZonaiPermafrostIA(PlayState* play, Player* this);
extern void Player_InitSwitchHookIA(PlayState* play, Player* this);
extern void Player_InitMogmaMittsIA(PlayState* play, Player* this);
extern void Player_InitWhipIA(PlayState* play, Player* this);
extern void Player_InitDominionRodIA(PlayState* play, Player* this);
extern void Player_InitTimeGateIA(PlayState* play, Player* this);
extern void Player_InitMinishCapIA(PlayState* play, Player* this);
extern void Player_InitLanternIA(PlayState* play, Player* this);
extern void Player_InitPending3IA(PlayState* play, Player* this);

// Decide whether an SW97 elemental arrow item should fire from bow or slingshot.
// Default: bow for adult, slingshot for child (vanilla age-based weapon).
// With BowSlingshotAmmoFix + TimelessEquipment both enabled, child can own and
// fire the bow — so prefer bow whenever the bow is in inventory regardless of age.
static s32 Sw97_PreferBow(void) {
    s32 useBow = LINK_IS_ADULT;
    if (CVarGetInteger(CVAR_ENHANCEMENT("BowSlingshotAmmoFix"), 0) &&
        CVarGetInteger(CVAR_CHEAT("TimelessEquipment"), 0)) {
        useBow = (INV_CONTENT(ITEM_BOW) == ITEM_BOW);
    }
    return useBow;
}

// ---------------------------------------------------------------------------
// Custom item descriptor table — single source of truth for engine glue.
//
//   item       ITEM_xxx, or NEI_NO_ITEM for IA-only rows (no inventory item).
//   ia         PLAYER_IA_xxx (unique per row).
//   modelGroup PLAYER_MODELGROUP_xxx.
//   slot       page-2 inventory slot (SLOT_*), or NEI_NO_SLOT.
//   ageReq     AGE_REQ_* (AGE_REQ_NONE for slotless rows).
//   icon       page-2 icon texture (NULL = dynamic, getter handles it).
//   updateFn   upper-action update (func_8083485C = generic "no special update").
//   initFn     action init (Player_InitDefaultIA = generic).
//
// Only items whose IA is a *custom* action live here; vanilla-IA aliases are
// resolved separately in ExtPlayer_GetItemAction. Skijer's NEI
// ---------------------------------------------------------------------------
// Skijer's NEI — extra columns: drawFunc (rando GI 3D model), rg (RandomizerGet,
// NEI_NO_RG if non-uniform / none), and name strings (relocated from
// customItemMessages[] so each item lives in one row). Roc's Feather Skijer keeps
// rg=NEI_NO_RG: ITEM_ROCS_FEATHER_SKIJER maps to two RGs (progressive + vanilla),
// so its give/draw/name stay on the old per-RG path.
static const NeiItem sNeiItems[] = {
    // item                          ia                              modelGroup                  slot                       ageReq         icon                                       update                          init                       drawFunc                          rg                       nameEn / nameFr / nameDe
    { ITEM_ROCS_FEATHER_SKIJER,      PLAYER_IA_ROCS_FEATHER_SKIJER,  PLAYER_MODELGROUP_DEFAULT,  SLOT_ROCS,                 AGE_REQ_NONE,  (void*)gItemIconRocsFeatherTex,            func_8083485C,                  Player_InitDefaultIA,      NULL,                             NEI_NO_RG,               NULL, NULL, NULL },
    { ITEM_ROCS_CAPE,                PLAYER_IA_ROCS_CAPE,            PLAYER_MODELGROUP_DEFAULT,  SLOT_ROCS,                 AGE_REQ_NONE,  (void*)gItemIconRocsCapeTex,               func_8083485C,                  Player_InitDefaultIA,      NULL,                             RG_ROCS_CAPE,
      "You got %rRoc's Cape%w!&This magical cape enhances&your jumping ability.^Now you can perform a&%gdouble jump%w "
      "in midair.&Press %y\xA1%w again while&jumping to go higher!",
      "Vous obtenez la %rCape de Roc%w!&Cette cape magique améliore&vos capacités de saut.^Vous pouvez maintenant "
      "effectuer&un %gdouble saut%w en l'air.&Appuyez sur %y\xA1%w en sautant&pour aller plus haut!",
      "Du hast %rRocs Umhang%w erhalten!&Dieser magische Umhang&verbessert deine Sprungkraft.^Du kannst nun "
      "einen&%gDoppelsprung%w in der Luft&ausführen. Drücke %y\xA1%w&erneut während du springst!" },
    { ITEM_DESIRE_SENSOR,            PLAYER_IA_DESIRE_SENSOR,        PLAYER_MODELGROUP_DEFAULT,  SLOT_DESIRE_SENSOR,        AGE_REQ_NONE,  (void*)gItemIconDesireSensorTex,           func_8083485C,                  Player_InitDefaultIA,      Randomizer_DrawDesireSensor,      RG_DESIRE_SENSOR,
      "You got the %pDesire Sensor%w!&A cursed artifact that reveals&hidden treasures... at a cost.^Press %y\xA1%w to "
      "activate.&%rCosts 3 hearts%w per use!^%g(Randomizer only)%w:&%yGolden sparkles%w = Major items&remain in this "
      "area.&%rGanondorf laugh%w = Nothing left.",
      "Vous obtenez le %pDétecteur de Désir%w!&Un artefact maudit qui révèle&les trésors cachés... à un prix.^Appuyez "
      "sur %y\xA1%w pour activer.&%rCoûte 3 cœurs%w par utilisation!^%g(Randomizer uniquement)%w:&%yÉtincelles dorées%w "
      "= Objets majeurs&restent dans cette zone.&%rRire de Ganondorf%w = Plus rien.",
      "Du hast den %pWunschdetektor%w!&Ein verfluchtes Artefakt das&verborgene Schätze enthüllt...&für einen "
      "Preis.^Drücke %y\xA1%w zum Aktivieren.&%rKostet 3 Herzen%w pro Nutzung!^%g(Nur im Randomizer)%w:&%yGoldene "
      "Funken%w = Wichtige Items&sind noch in diesem Gebiet.&%rGanondorfs Lachen%w = Nichts mehr da." },
    { ITEM_HYLIAS_GRACE,             PLAYER_IA_HYLIAS_GRACE,         PLAYER_MODELGROUP_DEFAULT,  SLOT_HYLIAS_GRACE,         AGE_REQ_NONE,  (void*)gItemIconHyliaGraceTex,             func_8083485C,                  Player_InitHyliasGraceIA,  Randomizer_DrawHyliaGrace,        RG_HYLIAS_GRACE,
      "You got %pHylia's Grace%w!&A divine blessing that transforms&you into a %cfairy%w for 10 seconds.^Press %y\xA1%w "
      "to activate&(requires a %rFairy in a Bottle%w).^%yA%w = Ascend  %yB%w = Descend&%yL%w = Sprint&1 minute "
      "cooldown after use.",
      "Vous obtenez la %pGrâce d'Hylia%w!&Une bénédiction divine qui vous&transforme en %cfée%w pendant 10 "
      "secondes.^Appuyez sur %y\xA1%w pour activer&(nécessite une %rFée en Bouteille%w).^%yA%w = Monter  %yB%w = "
      "Descendre&%yL%w = Sprint&1 minute de recharge après utilisation.",
      "Du hast %pHylias Gnade%w erhalten!&Ein göttlicher Segen der dich&für 10 Sekunden in eine %cFee%w "
      "verwandelt.^Drücke %y\xA1%w zum Aktivieren&(benötigt eine %rFee in einer Flasche%w).^%yA%w = Aufsteigen  %yB%w = "
      "Absteigen&%yL%w = Sprinten&1 Minute Abklingzeit nach Nutzung." },
    { ITEM_ZONAI_PERMAFROST,         PLAYER_IA_ZONAI_PERMAFROST,     PLAYER_MODELGROUP_DEFAULT,  SLOT_ZONAI_PERMAFROST,     AGE_REQ_NONE,  (void*)gItemIconZonaiPermafrostTex,        func_8083485C,                  Player_InitZonaiPermafrostIA, Randomizer_DrawZonaiPermafrost, RG_ZONAI_PERMAFROST,
      "You got %cZonai Permafrost%w!&Ancient Zonai technology that&freezes the flow of time itself.^Press %y\xA1%w to "
      "cast the spell.&%rAll enemies%w, %ypuzzle elements%w,&and even the %cday/night cycle%w&freeze for %g10 "
      "seconds%w!^Costs %g12 Magic%w per use.&Move freely while time is stopped.",
      "Vous obtenez %cPermafrost Soneau%w!&Technologie ancienne des Soneau&qui gèle le flux du temps.^Appuyez sur "
      "%y\xA1%w pour lancer&le sort. %rTous les ennemis%w,&%yéléments de puzzle%w, et même&le %ccycle jour/nuit%w "
      "gèlent&pendant %g10 secondes%w!^Coûte %g12 Magie%w par utilisation.&Bougez librement pendant que&le temps est "
      "arrêté.",
      "Du hast %cSonau Permafrost%w!&Uralte Sonau-Technologie die&den Fluss der Zeit einfriert.^Drücke %y\xA1%w um den "
      "Zauber&zu wirken. %rAlle Feinde%w,&%yRätsel-Elemente%w, und sogar&der %cTag/Nacht-Zyklus%w frieren&für %g10 "
      "Sekunden%w ein!^Kostet %g12 Magie%w pro Nutzung.&Bewege dich frei während die&Zeit angehalten ist." },
    { ITEM_DEMISE_DESTRUCTION,       PLAYER_IA_DEMISE_DESTRUCTION,   PLAYER_MODELGROUP_DEFAULT,  SLOT_DEMISE_DESTRUCTION,   AGE_REQ_NONE,  (void*)gItemIconDemiseDestructionTex,      func_8083485C,                  Player_InitDemiseDestructionIA, Randomizer_DrawDemiseDestruction, RG_DEMISE_DESTRUCTION,
      "You got %rDemise Destruction%w!&The dark power of the Demon King&Demise, sealed in this artifact.^Press %y\xA1%w "
      "to unleash a&devastating %rlightning explosion%w&that damages all enemies in&a %glarge radius%w around "
      "you.^%rHigh Magic cost%w.&Best saved for emergencies!&The ground itself trembles...",
      "Vous obtenez %rDestruction de l'Avatar%w!&Le pouvoir sombre du Roi Démon&Avatar, scellé dans cet "
      "artefact.^Appuyez sur %y\xA1%w pour déchaîner&une %rexplosion de foudre%w&dévastatrice qui blesse tous "
      "les&ennemis dans un %glarge rayon%w.^%rCoût élevé en Magie%w.&À garder pour les urgences!&La terre elle-même "
      "tremble...",
      "Du hast %rTodbringer Zerstörung%w!&Die dunkle Macht des Dämonenkönigs&Todbringer, versiegelt in "
      "diesem&Artefakt.^Drücke %y\xA1%w um eine verheerende&%rBlitz-Explosion%w zu entfesseln&die alle Feinde in "
      "einem&%ggroßen Radius%w um dich trifft.^%rHohe Magiekosten%w.&Am besten für Notfälle aufheben!&Der Boden selbst "
      "bebt..." },
    { ITEM_DEKU_LEAF,                PLAYER_IA_DEKU_LEAF,            PLAYER_MODELGROUP_DEFAULT,  SLOT_DEKU_LEAF,            AGE_REQ_CHILD, (void*)gItemIconDekuLeafTex,               Player_UpperAction_DekuLeaf,    Player_InitDefaultIA,      Randomizer_DrawDekuLeaf,          RG_DEKU_LEAF,
      "You got the %gDeku Leaf%w!&A giant leaf with powers&of the wind.^%yIn the air%w: Use it to glide&slowly and "
      "cover great&distances. Consumes magic.^%yOn the ground%w: Creates a gust&of wind that pushes objects&and "
      "enemies forward.",
      "Vous obtenez la %gFeuille Mojo%w!&Une feuille géante dotée&des pouvoirs du vent.^%yDans les airs%w: "
      "Planez&lentement sur de grandes&distances. Consomme de la magie.^%yAu sol%w: Crée une rafale&qui pousse les "
      "objets&et ennemis vers l'avant.",
      "Du hast das %gDeku-Blatt%w erhalten!&Ein Riesenblatt mit der&Kraft des Windes.^%yIn der Luft%w: Gleite "
      "langsam&und überbrücke große&Distanzen. Verbraucht Magie.^%yAm Boden%w: Erzeugt einen&Windstoß der Objekte "
      "und&Feinde nach vorne schiebt." },
    { ITEM_SWITCH_HOOK,              PLAYER_IA_SWITCH_HOOK,          PLAYER_MODELGROUP_HOOKSHOT, SLOT_SWITCH_HOOK,          AGE_REQ_CHILD, (void*)gItemIconSwitchHookTex,             Player_UpperAction_SwitchHook,  Player_InitSwitchHookIA,   Randomizer_DrawSwitchHook,        RG_SWITCH_HOOK,
      "You got the %cSwitch Hook%w!&A magical hook that swaps&your position with targets.^Hold %y\xA1%w to aim,&release "
      "to fire.&%c\xA5%w = First-person mode^Swap places with pots, crates,&and certain enemies!&Non-swappable targets "
      "take damage.",
      "Vous obtenez le %cCrochet Échange%w!&Un crochet magique qui échange&votre position avec les cibles.^Maintenez "
      "%y\xA1%w pour viser,&relâchez pour tirer.&%c\xA5%w = Première personne^Échangez avec des pots, caisses,&et "
      "certains ennemis!&Les cibles non-échangeables subissent des dégâts.",
      "Du hast den %cWechselhaken%w!&Ein magischer Haken der deine&Position mit Zielen tauscht.^Halte %y\xA1%w zum "
      "Zielen,&lass los zum Feuern.&%c\xA5%w = Erste-Person^Tausche Plätze mit Töpfen, Kisten&und bestimmten "
      "Feinden!&Nicht-tauschbare Ziele nehmen Schaden." },
    { ITEM_MOGMA_MITTS,              PLAYER_IA_MOGMA_MITTS,          PLAYER_MODELGROUP_DEFAULT,  SLOT_MOGMA_MITTS,          AGE_REQ_NONE,  (void*)gItemIconMogmaMittsTex,             func_8083485C,                  Player_InitMogmaMittsIA,   Randomizer_DrawMogmaMitts,        RG_MOGMA_MITTS,
      "You got the %yMogma Mitts%w!&Claws of the underground.&Climb any wall! Uses %gMagic%w.",
      "Vous obtenez les %yGants Mogma%w!&Griffes souterraines.&Grimpez partout! Utilise de la %gMagie%w.",
      "Du hast die %yMogma-Klauen%w erhalten!&Klauen aus dem Untergrund.&Klettere überall! Verbraucht %gMagie%w." },
    { ITEM_GUST_JAR,                 PLAYER_IA_GUST_JAR,             PLAYER_MODELGROUP_DEFAULT,  SLOT_GUST_JAR,             AGE_REQ_CHILD, (void*)gItemIconGustJarTex,                func_8083485C,                  Player_InitGustJarIA,      Randomizer_DrawGustJar,           RG_GUST_JAR,
      "You got the %gGust Jar%w!&A vessel containing&ancient winds.^%ySuction mode%w: Hold %y\xA1%w&to absorb objects, "
      "enemies&and environmental elements.^%yCapture mode%w: Absorb fire,&ice or electricity to store&special "
      "ammunition.^%yShoot mode%w: Release %y\xA1%w to&fire what you captured.&%c\xA5%w = First-person mode",
      "Vous obtenez le %gPot Magique%w!&Un récipient contenant&des vents anciens.^%yMode aspiration%w: Maintenez "
      "%y\xA1%w&pour absorber objets, ennemis&et éléments environnementaux.^%yMode capture%w: Absorbez feu,&glace ou "
      "électricité comme&munition spéciale.^%yMode tir%w: Relâchez %y\xA1%w pour&tirer ce que vous avez "
      "capturé.&%c\xA5%w = Première personne",
      "Du hast den %gMagischen Krug%w!&Ein Gefäß mit uralten&Winden.^%yAnsaugmodus%w: Halte %y\xA1%w&um Objekte, Feinde "
      "und&Umgebungselemente anzusaugen.^%yFangmodus%w: Sauge Feuer,&Eis oder Elektrizität auf&als spezielle "
      "Munition.^%ySchussmodus%w: Lass %y\xA1%w los&um das Gefangene zu feuern.&%c\xA5%w = Erste-Person" },
    { ITEM_BALL_AND_CHAIN,           PLAYER_IA_BALL_AND_CHAIN,       PLAYER_MODELGROUP_DEFAULT,  SLOT_BALL_AND_CHAIN,       AGE_REQ_ADULT, (void*)gItemIconBallAndChainTex,           func_8083485C,                  Player_InitBallAndChainIA, Randomizer_DrawBallAndChain,      RG_BALL_AND_CHAIN,
      "You got the %yBall and Chain%w!&A heavy weapon from the&snow palace.^Hold %y\xA1%w to charge,&release to "
      "throw.&Crush ice and enemies!^With %g\xA4%w it homes in&on the enemy automatically.&Breaks %rRed "
      "Ice%w!^%rNote%w: Your speed is reduced&while it's equipped.",
      "Vous obtenez le %yBoulet%w!&Une arme lourde du palais&des neiges.^Maintenez %y\xA1%w pour charger,&relâchez pour "
      "lancer.&Écrasez glace et ennemis!^Avec %g\xA4%w il suit&automatiquement l'ennemi.&Brise la %rGlace "
      "Rouge%w!^%rNote%w: Votre vitesse est réduite&tant qu'il est équipé.",
      "Du hast die %yKettenkugel%w!&Eine schwere Waffe aus dem&Schneepalast.^Halte %y\xA1%w zum Aufladen,&lass los zum "
      "Werfen.&Zerschmettere Eis und Feinde!^Mit %g\xA4%w verfolgt sie&automatisch den Feind.&Zerbricht %rRotes "
      "Eis%w!^%rHinweis%w: Deine Geschwindigkeit&ist reduziert während sie&ausgerüstet ist." },
    { ITEM_WHIP,                     PLAYER_IA_WHIP,                 PLAYER_MODELGROUP_DEFAULT,  SLOT_WHIP,                 AGE_REQ_NONE,  (void*)gItemIconWhipTex,                   func_8083485C,                  Player_InitWhipIA,         Randomizer_DrawWhip,              RG_WHIP,
      "You got the %yWhip%w!&A versatile tool for combat&and exploration.^Press %y\xA1%w to lash forward.&It latches "
      "onto beams and bars&for pendulum swinging.^%ySwinging%w: Use the stick to&control the pendulum.&Release to "
      "launch with momentum!^%yCombat%w: Paralyze enemies,&pull shields, and disarm.&Also grabs items!",
      "Vous obtenez le %yFouet%w!&Un outil polyvalent pour le combat&et l'exploration.^Appuyez sur %y\xA1%w pour "
      "fouetter.&S'accroche aux poutres et barres&pour se balancer en pendule.^%yBalancement%w: Utilisez le stick&pour "
      "contrôler le pendule.&Relâchez pour vous lancer!^%yCombat%w: Paralysez les ennemis,&tirez les boucliers et "
      "désarmez.&Attrape aussi des objets!",
      "Du hast die %yPeitsche%w!&Ein vielseitiges Werkzeug für&Kampf und Erkundung.^Drücke %y\xA1%w zum Schlagen.&Hakt "
      "sich an Balken und Stangen&zum Pendelschwingen ein.^%ySchwingen%w: Nutze den Stick um&das Pendel zu "
      "steuern.&Lass los für Schwung-Start!^%yKampf%w: Lähme Feinde,&ziehe Schilde weg und entwaffne.&Greift auch "
      "Items!" },
    { ITEM_SPINNER,                  PLAYER_IA_SPINNER,              PLAYER_MODELGROUP_DEFAULT,  SLOT_SPINNER,              AGE_REQ_NONE,  (void*)gItemIconSpinnerTex,                func_8083485C,                  Player_InitSpinnerIA,      Randomizer_DrawSpinner,           RG_SPINNER,
      "You got the %ySpinner%w!&Ancient technology from the&desert sands.^Press %y\xA1%w to ride it&and glide around. "
      "Use it to&cross great distances.^With %g\xA4%w you perform&a homing attack towards&the enemy. Breaks rocks!",
      "Vous obtenez la %yToupie%w!&Technologie ancienne des&sables du désert.^Appuyez sur %y\xA1%w pour monter&et "
      "glisser. Utilisez-la pour&traverser de grandes distances.^Avec %g\xA4%w vous effectuez&une attaque guidée "
      "vers&l'ennemi. Brise les rochers!",
      "Du hast den %yKreisel%w!&Uralte Technologie aus dem&Wüstensand.^Drücke %y\xA1%w um aufzusteigen&und zu gleiten. "
      "Überbrücke&große Distanzen damit.^Mit %g\xA4%w führst du einen&Verfolgungs-Angriff auf&den Feind aus. "
      "Zerbricht Felsen!" },
    { ITEM_CANE_OF_SOMARIA,          PLAYER_IA_CANE_OF_SOMARIA,      PLAYER_MODELGROUP_DEFAULT,  SLOT_CANE_OF_SOMARIA,      AGE_REQ_NONE,  (void*)gItemIconCaneOfSomariaTex,          Player_UpperAction_CaneOfSomaria, Player_InitCaneOfSomariaIA, Randomizer_DrawCaneOfSomaria,   RG_CANE_OF_SOMARIA,
      "You got the %rCane of Somaria%w!&A wand that creates magical&blocks out of thin air.^Press %y\xA1%w to swing and "
      "create&a %rmagical block%w. Up to %g3&blocks%w can exist at once.^The %roldest block%w is destroyed&when you "
      "create a 4th.^Use them to activate switches,&block enemies, or as&platforms to reach heights.",
      "Vous obtenez la %rCanne de Somaria%w!&Une baguette qui crée des&blocs magiques de nulle part.^Appuyez sur "
      "%y\xA1%w pour brandir&et créer un %rbloc magique%w.&Jusqu'à %g3 blocs%w peuvent exister.^Le %rbloc le plus "
      "ancien%w est&détruit quand vous en créez un 4e.^Utilisez-les pour activer des&interrupteurs, bloquer des "
      "ennemis,&ou comme plateformes.",
      "Du hast den %rStab von Somaria%w!&Ein Stab der magische Blöcke&aus dem Nichts erschafft.^Drücke %y\xA1%w zum "
      "Schwingen&und erschaffe einen %rmagischen&Block%w. Bis zu %g3 Blöcke%w können&gleichzeitig existieren.^Der "
      "%rälteste Block%w wird zerstört&wenn du einen 4. erschaffst.^Nutze sie für Schalter, um Feinde&zu blockieren, "
      "oder als Plattform." },
    { ITEM_DOMINION_ROD,             PLAYER_IA_DOMINION_ROD,         PLAYER_MODELGROUP_DEFAULT,  SLOT_DOMINION_ROD,         AGE_REQ_NONE,  (void*)gItemIconDominionRodTex,            func_8083485C,                  Player_InitDominionRodIA,  Randomizer_DrawDominionRod,       RG_DOMINION_ROD,
      "You got the %pDominion Rod%w!&An ancient artifact that can&possess and control enemies.^Press %y\xA1%w to fire a "
      "golden orb.&It can possess: %rBeamos%w,&%yArmos%w, and %cAnubis%w.^Once possessed, the enemy will&%gmimic your "
      "movements%w!&Walk to make it walk,&attack to make it attack.^Uses %gMagic%w while controlling.",
      "Vous obtenez la %pBaguette des Animes%w!&Un artefact ancien qui peut&posséder et contrôler les ennemis.^Appuyez "
      "sur %y\xA1%w pour tirer un&orbe doré. Il peut posséder:&%rBeamos%w, %yArmos%w et %cAnubis%w.^Une fois possédé, "
      "l'ennemi va&%gimiter vos mouvements%w!&Marchez pour le faire marcher,&attaquez pour le faire attaquer.^Utilise "
      "de la %gMagie%w pendant&le contrôle.",
      "Du hast den %pKopierstab%w!&Ein uraltes Artefakt das Feinde&besitzen und kontrollieren kann.^Drücke %y\xA1%w um "
      "einen goldenen Orb&zu feuern. Er kann besitzen:&%rBeamos%w, %yArmos%w und %cAnubis%w.^Einmal besessen, wird der "
      "Feind&%gdeine Bewegungen imitieren%w!&Laufe um ihn laufen zu lassen,&greife an um ihn angreifen zu "
      "lassen.^Verbraucht %gMagie%w beim Kontrollieren." },
    { ITEM_TIME_GATE,                PLAYER_IA_TIME_GATE,            PLAYER_MODELGROUP_DEFAULT,  SLOT_TIME_GATE,            AGE_REQ_NONE,  (void*)gItemIconTimeGateTex,               func_8083485C,                  Player_InitTimeGateIA,     Randomizer_DrawTimeGate,          RG_TIME_GATE,
      "You got the %cTime Gate%w!&A portable door through the ages,&the power of the Temple of Time&in your "
      "hands.^Press %y\xA1%w to activate.&A prompt will ask: %g\"Travel&through time?\"%w^Select %yYes%w to switch "
      "between&%rChild%w and %gAdult%w Link&anywhere in the world!^Costs %g48 Magic%w per use.",
      "Vous obtenez la %cPorte du Temps%w!&Une porte portable à travers les&âges, le pouvoir du Temple du Temps&dans "
      "vos mains.^Appuyez sur %y\xA1%w pour activer.&Une question apparaît: %g\"Voyager&dans le temps?\"%w^Sélectionnez "
      "%yOui%w pour passer&entre Link %rEnfant%w et %gAdulte%w&n'importe où!^Coûte %g48 Magie%w par utilisation.",
      "Du hast das %cZeittor%w!&Eine tragbare Tür durch die Zeit,&die Macht des Zeitturms in&deinen Händen.^Drücke "
      "%y\xA1%w zum Aktivieren.&Eine Frage erscheint: %g\"Durch&die Zeit reisen?\"%w^Wähle %yJa%w um zwischen&%rKind%w "
      "und %gErwachsenem%w Link&überall zu wechseln!^Kostet %g48 Magie%w pro Nutzung." },
    { ITEM_BOMB_ARROWS,              PLAYER_IA_BOMB_ARROWS,          PLAYER_MODELGROUP_DEFAULT,  SLOT_BOMB_ARROWS,          AGE_REQ_ADULT, (void*)gItemIconBombArrowsTex,             Player_UpperAction_BombArrows,  Player_InitBombArrowsIA,   Randomizer_DrawBombArrows,        RG_BOMB_ARROWS,
      "You got %rBomb Arrows%w!&An explosive combination.^Requires %yArrows%w and %rBombs%w.&Use %y\xA1%w to enter "
      "first-person&mode and aim.^The arrow explodes on impact.&Consumes %y1 arrow%w + %r1 bomb%w&per shot.",
      "Vous obtenez les %rFlèches-Bombes%w!&Une combinaison explosive.^Nécessite des %yFlèches%w et "
      "%rBombes%w.&Utilisez %y\xA1%w pour entrer en&première personne et viser.^La flèche explose à l'impact.&Consomme "
      "%y1 flèche%w + %r1 bombe%w&par tir.",
      "Du hast %rBombenpfeile%w!&Eine explosive Kombination.^Benötigt %yPfeile%w und %rBomben%w.&Benutze %y\xA1%w für "
      "Erste-Person&Modus und zielen.^Der Pfeil explodiert beim&Aufprall. Verbraucht %y1 Pfeil%w&+ %r1 Bombe%w pro "
      "Schuss." },
    // Rods use the BGS (two-handed) model group + sword mechanics for charge attacks.
    { ITEM_ROD_FIRE,                 PLAYER_IA_ROD_FIRE,             PLAYER_MODELGROUP_BGS,      SLOT_FIRE_ROD,             AGE_REQ_NONE,  (void*)gItemIconFireRodTex,                Player_UpperAction_Sword,       Player_InitFireRodIA,      Randomizer_DrawFireRod,           RG_FIRE_ROD,
      "You got the %rFire Rod%w!&A magical weapon that channels&the power of fire.^%yBasic attacks%w:&Slash = 3 "
      "fireballs&Stab = 1 fireball&Jump = Flamethrower down^%ySpecial attacks%w:&Spin = Expanding fire wave&Hold "
      "%y\xA1%w = Charge attack&%c\xA5%w = First-person mode^%rWarning%w: Without magic, the&fire will burn YOU. Make "
      "sure&you have enough magic!",
      "Vous obtenez la %rBaguette de Feu%w!&Une arme magique qui canalise&le pouvoir du feu.^%yAttaques de "
      "base%w:&Taille = 3 boules de feu&Estoc = 1 boule de feu&Saut = Lance-flammes^%yAttaques spéciales%w:&Tourbillon "
      "= Vague de feu&Maintenez %y\xA1%w = Charge&%c\xA5%w = Première personne^%rAttention%w: Sans magie, le feu&VOUS "
      "brûlera. Assurez-vous&d'avoir assez de magie!",
      "Du hast den %rFeuerstab%w!&Eine magische Waffe mit der&Kraft des Feuers.^%yBasisangriffe%w:&Hieb = 3 "
      "Feuerbälle&Stoß = 1 Feuerball&Sprung = Flammenwerfer^%ySpezialangriffe%w:&Wirbelattacke = Feuerwelle&Halte "
      "%y\xA1%w = Aufladen&%c\xA5%w = Erste-Person^%rWarnung%w: Ohne Magie verbrennt&das Feuer DICH. Achte auf&genug "
      "Magie!" },
    { ITEM_ROD_ICE,                  PLAYER_IA_ROD_ICE,              PLAYER_MODELGROUP_BGS,      SLOT_ICE_ROD,              AGE_REQ_NONE,  (void*)gItemIconIceRodTex,                 Player_UpperAction_Sword,       Player_InitIceRodIA,       Randomizer_DrawIceRod,            RG_ICE_ROD,
      "You got the %bIce Rod%w!&A magical weapon that channels&the power of ice.^%yBasic attacks%w:&Slash = 3 ice "
      "projectiles&Stab = 1 ice projectile&Jump = Freezing blast down^%ySpecial attacks%w:&Spin = Expanding ice "
      "wave&Hold %y\xA1%w = Charge attack&%c\xA5%w = First-person mode^%rWarning%w: Without magic, the&ice will freeze "
      "YOU. Make sure&you have enough magic!",
      "Vous obtenez la %bBaguette de Glace%w!&Une arme magique qui canalise&le pouvoir de la glace.^%yAttaques de "
      "base%w:&Taille = 3 projectiles de glace&Estoc = 1 projectile de glace&Saut = Souffle glacial^%yAttaques "
      "spéciales%w:&Tourbillon = Vague de glace&Maintenez %y\xA1%w = Charge&%c\xA5%w = Première "
      "personne^%rAttention%w: Sans magie, la glace&VOUS gèlera. Assurez-vous&d'avoir assez de magie!",
      "Du hast den %bEisstab%w!&Eine magische Waffe mit der&Kraft des Eises.^%yBasisangriffe%w:&Hieb = 3 "
      "Eisprojektile&Stoß = 1 Eisprojektil&Sprung = Eisstrahl^%ySpezialangriffe%w:&Wirbelattacke = Eiswelle&Halte "
      "%y\xA1%w = Aufladen&%c\xA5%w = Erste-Person^%rWarnung%w: Ohne Magie friert&das Eis DICH ein. Achte auf&genug "
      "Magie!" },
    { ITEM_ROD_LIGHT,                PLAYER_IA_ROD_LIGHT,            PLAYER_MODELGROUP_BGS,      SLOT_LIGHT_ROD,            AGE_REQ_NONE,  (void*)gItemIconLightRodTex,               Player_UpperAction_Sword,       Player_InitLightRodIA,     Randomizer_DrawLightRod,          RG_LIGHT_ROD,
      "You got the %yLight Rod%w!&A magical weapon that channels&the power of lightning.^%yBasic attacks%w:&Slash = 3 "
      "lightning bolts&Stab = 1 lightning bolt&Jump = Electric discharge^%ySpecial attacks%w:&Spin = Expanding "
      "electric wave&Hold %y\xA1%w = Charge attack&%c\xA5%w = First-person mode^%rWarning%w: Without magic, "
      "the&lightning will shock YOU.&Make sure you have enough magic!",
      "Vous obtenez la %yBaguette de Lumière%w!&Une arme magique qui canalise&le pouvoir de la foudre.^%yAttaques de "
      "base%w:&Taille = 3 éclairs en éventail&Estoc = 1 éclair direct&Saut = Décharge électrique^%yAttaques "
      "spéciales%w:&Tourbillon = Vague électrique&Maintenez %y\xA1%w = Charge&%c\xA5%w = Première "
      "personne^%rAttention%w: Sans magie, la foudre&VOUS électrocutera. Assurez-vous&d'avoir assez de magie!",
      "Du hast den %yLichtstab%w!&Eine magische Waffe mit der&Kraft des Blitzes.^%yBasisangriffe%w:&Hieb = 3 Blitze im "
      "Bogen&Stoß = 1 direkter Blitz&Sprung = Elektrische Entladung^%ySpezialangriffe%w:&Wirbelattacke = "
      "Elektrowelle&Halte %y\xA1%w = Aufladen&%c\xA5%w = Erste-Person^%rWarnung%w: Ohne Magie trifft&der Blitz DICH. "
      "Achte auf&genug Magie!" },
    { ITEM_BEETLE,                   PLAYER_IA_BEETLE,               PLAYER_MODELGROUP_DEFAULT,  SLOT_BEETLE,               AGE_REQ_ADULT, (void*)gItemIconBeetleTex,                 Player_UpperAction_Beetle,      Player_InitBeetleIA,       Randomizer_DrawBeetle,            RG_BEETLE,
      "You got the %gBeetle%w!&A remote-controlled mechanical&insect from ancient times.^%y\xA1%w = Launch "
      "beetle&%yAnalog Stick%w = Steer flight&%y\xA1%w again = Recall beetle&%y\xA0%w = Speed boost^The camera follows "
      "the beetle.&Use it to grab distant items,&hit switches, and scout ahead!",
      "Vous obtenez le %gScarabée%w!&Un insecte mécanique télécommandé&des temps anciens.^%y\xA1%w = Lancer le "
      "scarabée&%yStick Analogique%w = Diriger le vol&%y\xA1%w à nouveau = Rappeler&%y\xA0%w = Accélération^La caméra "
      "suit le scarabée.&Utilisez-le pour attraper des objets,&activer des interrupteurs et explorer!",
      "Du hast den %gKäfer%w erhalten!&Ein ferngesteuertes mechanisches&Insekt aus alter Zeit.^%y\xA1%w = Käfer "
      "starten&%yAnalog-Stick%w = Flug steuern&%y\xA1%w erneut = Käfer zurückrufen&%y\xA0%w = Geschwindigkeitsschub^Die "
      "Kamera folgt dem Käfer.&Nutze ihn um Items zu holen,&Schalter zu treffen und voraus zu spähen!" },
    { ITEM_SHOVEL,                   PLAYER_IA_SHOVEL,               PLAYER_MODELGROUP_DEFAULT,  SLOT_SHOVEL,               AGE_REQ_NONE,  (void*)gItemIconShovelTex,                 Player_UpperAction_Shovel,      Player_InitDefaultIA,      Randomizer_DrawShovel,            RG_SHOVEL,
      "You got the %yShovel%w!&A reliable tool for&excavation.^Use %y\xA1%w on soft soil&to dig and find "
      "hidden&treasures.^It can also reveal secret&%gGrottos%w and damage&buried enemies!",
      "Vous obtenez la %yPelle%w!&Un outil fiable pour&l'excavation.^Utilisez %y\xA1%w sur terre&meuble pour creuser "
      "et&trouver des trésors cachés.^Elle peut aussi révéler des&%gGrottes secrètes%w et blesser&les ennemis "
      "enterrés!",
      "Du hast die %ySchaufel%w!&Ein zuverlässiges Werkzeug&zum Graben.^Benutze %y\xA1%w auf weichem&Boden um zu graben "
      "und&verborgene Schätze zu finden.^Sie kann auch geheime&%gGrotten%w aufdecken und&vergrabene Feinde verletzen!" },
    { ITEM_MINISH_CAP,               PLAYER_IA_MINISH_CAP,           PLAYER_MODELGROUP_DEFAULT,  SLOT_MINISH_CAP,           AGE_REQ_CHILD, (void*)gItemIconMinishCapTex,              func_8083485C,                  Player_InitMinishCapIA,    Randomizer_DrawMinishCap,         RG_PENDING_1,
      "You got %pThe Minish Cap%w!&Fast travel between pod soils.",
      "Vous obtenez %pPending Item 1%w!&Cet objet n'est pas encore implémenté.",
      "Du hast %pThe Minish Cap%w!&Schnellreise zwischen Pod Soils." },
    // Lantern: icon is dynamic (chosen by fire type) -> NULL, getter handles it. Skijer's NEI
    { ITEM_LANTERN,                  PLAYER_IA_LANTERN,              PLAYER_MODELGROUP_DEFAULT,  SLOT_LANTERN,              AGE_REQ_NONE,  NULL,                                      func_8083485C,                  Player_InitLanternIA,      Randomizer_DrawLantern,           RG_LANTERN,
      "You got the %yLantern%w!&Catch fire from torches and&use it to light your way!",
      "Vous obtenez la %yLanterne%w!&Capturez le feu des torches et&utilisez-le pour éclairer votre chemin!",
      "Du hast die %yLaterne%w erhalten!&Fang Feuer von Fackeln und&nutze es um deinen Weg zu erleuchten!" },
    // Pokéball maps onto the PENDING_3 action slot.
    { ITEM_POKEBALL,                 PLAYER_IA_PENDING_3,            PLAYER_MODELGROUP_DEFAULT,  SLOT_PENDING_3,            AGE_REQ_NONE,  (void*)gItemIconPokeballTex,               func_8083485C,                  Player_InitPending3IA,     Randomizer_DrawPokeball,          RG_PENDING_3,
      "You got the %yPoké Ball%w!&Use it to give orders to&a transformed Pikachu."
      "^%y\x9F%w combo  %y\xA0%w Thunder Jolt&Stick+%y\x9F%w/%y\xA0%w: smash / special&%y\xA2%w crouch  %y\xA3%w bubble shield&%y\xA1%w-buttons: special items",
      "Vous obtenez la %yPoké Ball%w!&Donnez des ordres à un&Pikachu transformé."
      "^%y\x9F%w combo  %y\xA0%w Tonnerre&Stick+%y\x9F%w/%y\xA0%w: smash / spécial&%y\xA2%w accroupi  %y\xA3%w bouclier&%y\xA1%w: objets spéciaux",
      "Du hast den %yPokéball%w erhalten!&Damit gibst du einem&verwandelten Pikachu Befehle."
      "^%y\x9F%w Combo  %y\xA0%w Donner-Schock&Stick+%y\x9F%w/%y\xA0%w: Smash / Special&%y\xA2%w Hocken  %y\xA3%w Blasen-Schild&%y\xA1%w-Tasten: Special-Items" },
    // IA-only: reserved action with no inventory item (default behavior).
    { NEI_NO_ITEM,                   PLAYER_IA_UNUSED_5B,            PLAYER_MODELGROUP_DEFAULT,  NEI_NO_SLOT,               AGE_REQ_NONE,  NULL,                                      func_8083485C,                  Player_InitDefaultIA,      NULL,                             NEI_NO_RG,               NULL, NULL, NULL },
    // Bottle with Magic Mushroom — bottle behavior (drop on B-swing via vanilla path). Give stays on old path (bottle-loop before the switch).
    { ITEM_BOTTLE_WITH_MAGIC_MUSHROOM, PLAYER_IA_BOTTLE_MAGIC_MUSHROOM, PLAYER_MODELGROUP_DEFAULT, NEI_NO_SLOT,            AGE_REQ_NONE,  NULL,                                      func_8083485C,                  Player_InitDefaultIA,      Randomizer_DrawBottleWithMagicMushroom, RG_BOTTLE_WITH_MAGIC_MUSHROOM,
      "You got a %gBottle with Magic Mushroom%w!&A fragrant Termina mushroom plucked&by the keen nose of the Mask of Scents.^Stored in an empty bottle.&Drop it later for unknown effects -&or simply admire the catch.",
      "Vous obtenez une %gFiole avec Champignon Magique%w!&Un champignon parfumé de Termina,&flairé par le Masque des Odeurs.^Stocké dans une fiole vide.&À déposer plus tard pour des effets&inconnus - ou à contempler.",
      "Du hast eine %gFlasche mit Zauberpilz%w!&Ein duftender Termina-Pilz, geschnüffelt&von der Geruchsmaske.^In einer leeren Flasche aufbewahrt.&Lass ihn später fallen für unbekannte&Effekte - oder bewundere ihn." },

    // MM Mask IAs (all no-op: default model, generic update + init). Page-3 slots + icons stay on gPage3Mask* tables.
    // All 24 share Randomizer_DrawMmMask (dispatches by RG internally). Names relocated from customItemMessages[].
    { ITEM_MM_MASK_POSTMAN,      PLAYER_IA_MM_MASK_POSTMAN,       PLAYER_MODELGROUP_DEFAULT, NEI_NO_SLOT, AGE_REQ_NONE, NULL, func_8083485C, Player_InitDefaultIA, Randomizer_DrawMmMask, RG_MM_MASK_POSTMAN,
      "You got the %yPostman's Hat%w!&The official cap of Termina's&most punctual courier.^Equip from the mask page.^Walk up to any %gunlocked mailbox%w&and press %y\xA0%w to open the&%cMailbox Warp Menu%w - fast travel&to any other unlocked mailbox.",
      "Vous obtenez le %yChapeau du Facteur%w!&Le képi officiel du courrier le&plus ponctuel de Termina.^Équipez depuis la page des masques.^Approchez n'importe quelle %gboîte aux&lettres débloquée%w et %y\xA0%w pour ouvrir&le %cMenu de Téléportation%w - voyage&rapide vers toute autre boîte.",
      "Du hast den %yBriefträgerhut%w!&Die offizielle Mütze von Terminas&pünktlichstem Boten.^Aufsetzen auf der Maskenseite.^Geh zu einem %gfreigeschalteten Briefkasten%w&und drücke %y\xA0%w für das&%cBriefkasten-Warp-Menü%w - Schnellreise&zu jedem anderen freigeschalteten Briefkasten." },
    { ITEM_MM_MASK_ALL_NIGHT,    PLAYER_IA_MM_MASK_ALL_NIGHT,     PLAYER_MODELGROUP_DEFAULT, NEI_NO_SLOT, AGE_REQ_NONE, NULL, func_8083485C, Player_InitDefaultIA, Randomizer_DrawMmMask, RG_MM_MASK_ALL_NIGHT,
      "You got the %yAll-Night Mask%w!&A mask said to grant insomnia&and the gift of seeing in the dark.^Equip from the mask page.^While worn during %gdaytime%w, all&%cnight-only Gold Skulltulas%w spawn&as if it were night - Graveyard,&Zora's Fountain, Gerudo Fortress,&Kakariko, and Lon Lon Ranch.",
      "Vous obtenez le %yMasque de Nuit%w!&Un masque qui octroierait l'insomnie&et le don de voir dans l'obscurité.^Équipez depuis la page des masques.^Porté de %gjour%w, toutes les&%cSkulltulas d'Or de nuit%w apparaissent&comme s'il faisait nuit - Cimetière,&Fontaine Zora, Forteresse Gerudo,&Kakariko et Ranch Lon Lon.",
      "Du hast die %yNachtmaske%w!&Eine Maske, die Schlaflosigkeit&und Nachtsicht verleihen soll.^Aufsetzen auf der Maskenseite.^Beim Tragen am %gTag%w erscheinen alle&%cnur-nachts Goldskulltulas%w, als wäre&es Nacht - Friedhof, Zora-Quelle,&Gerudo-Festung, Kakariko und&Lon Lon Ranch." },
    { ITEM_MM_MASK_BLAST,        PLAYER_IA_MM_MASK_BLAST,         PLAYER_MODELGROUP_DEFAULT, NEI_NO_SLOT, AGE_REQ_NONE, NULL, func_8083485C, Player_InitDefaultIA, Randomizer_DrawMmMask, RG_MM_MASK_BLAST,
      "You got the %yBlast Mask%w!&A mask of explosive power born&of pure detonation.^Equip from the mask page.^%y\xA0%w detonates an %rinstant explosion%w&at Link's position - no bombs needed.&Cooldown: %g~310 frames%w (~16 s).&With %cgMods.BlastMask.Instant%w on,&cooldown drops to 1 frame.",
      "Vous obtenez le %yMasque d'Explosion%w!&Un masque de pure détonation&aux pouvoirs explosifs.^Équipez depuis la page des masques.^%y\xA0%w déclenche une %rexplosion instantanée%w&à la position de Link - aucune bombe.&Recharge: %g~310 frames%w (~16 s).&Avec %cgMods.BlastMask.Instant%w activé,&la recharge tombe à 1 frame.",
      "Du hast die %yExplosionsmaske%w!&Eine Maske explosiver Kraft,&geboren aus reiner Detonation.^Aufsetzen auf der Maskenseite.^%y\xA0%w zündet eine %rsofortige Explosion%w&an Links Position - keine Bomben nötig.&Abklingzeit: %g~310 Frames%w (~16 s).&Mit %cgMods.BlastMask.Instant%w an,&fällt die Abklingzeit auf 1 Frame." },
    { ITEM_MM_MASK_STONE,        PLAYER_IA_MM_MASK_STONE,         PLAYER_MODELGROUP_DEFAULT, NEI_NO_SLOT, AGE_REQ_NONE, NULL, func_8083485C, Player_InitDefaultIA, Randomizer_DrawMmMask, RG_MM_MASK_STONE,
      "You got the %yStone Mask%w!&A featureless gray mask said to&render its wearer beneath notice.^Equip from the mask page.^While worn, %cenemies cannot see you%w&- they will not target you,&aggro you, or react to your&presence at all. Stealth pure.",
      "Vous obtenez le %yMasque de Pierre%w!&Un masque gris sans visage qui&rend son porteur invisible.^Équipez depuis la page des masques.^Pendant le port, %cles ennemis ne&peuvent pas vous voir%w - ils ne&vous ciblent pas, ne deviennent pas&agressifs, ne réagissent pas. Furtivité pure.",
      "Du hast die %ySteinmaske%w!&Eine merkmallose graue Maske, die&ihren Träger unsichtbar macht.^Aufsetzen auf der Maskenseite.^Beim Tragen können dich %cFeinde&nicht sehen%w - sie zielen nicht&auf dich, werden nicht aggressiv&und reagieren nicht auf dich. Reine Tarnung." },
    { ITEM_MM_MASK_GREAT_FAIRY,  PLAYER_IA_MM_MASK_GREAT_FAIRY,   PLAYER_MODELGROUP_DEFAULT, NEI_NO_SLOT, AGE_REQ_NONE, NULL, func_8083485C, Player_InitDefaultIA, Randomizer_DrawMmMask, RG_MM_MASK_GREAT_FAIRY,
      "You got the %yGreat Fairy Mask%w!&A wreath of long pink hair&blessed by the fairies.^Equip from the mask page.&In a fairy fountain, %y\xA0%w claims&the Great Fairy reward.^Press %y\xA1%w anywhere to open the&%cFairy Warp Menu%w - teleport to&any unlocked Great Fairy fountain.&Hair physics flow as you move.",
      "Vous obtenez le %yMasque de la Grande Fée%w!&Une couronne de longs cheveux roses&bénie par les fées.^Équipez depuis la page des masques.&Dans une fontaine, %y\xA0%w réclame&la récompense de la Grande Fée.^%y\xA1%w n'importe où ouvre le&%cMenu de Téléportation%w - voyagez&vers toute fontaine débloquée.&Physique de cheveux en mouvement.",
      "Du hast die %yFeenmaske%w!&Ein Kranz langer rosa Haare,&von den Feen gesegnet.^Aufsetzen auf der Maskenseite.&In einer Feenquelle %y\xA0%w drücken,&um die Belohnung zu erhalten.^Drücke %y\xA1%w überall für das&%cFeen-Warp-Menü%w - teleportiere&zu jeder freigeschalteten Feenquelle.&Haar-Physik beim Bewegen." },
    { ITEM_MM_MASK_DEKU,         PLAYER_IA_MM_MASK_DEKU,          PLAYER_MODELGROUP_DEFAULT, NEI_NO_SLOT, AGE_REQ_NONE, NULL, func_8083485C, Player_InitDefaultIA, Randomizer_DrawMmMask, RG_MM_MASK_DEKU,
      "You got the %gDeku Mask%w!&Holds the spirit of a fallen&Deku Scrub.^Equip from the mask page -&Link transforms into a small,&light Deku Scrub.^%y\xA0%w spin attack (pn_attack).&Hold %y\xA0%w to aim -> release fires&a %gbubble projectile%w (costs Magic).^Stand on a %gDeku Flower%w + %y\xA0%w to&burrow, charge, then launch into&a finite-distance %gglide%w.^%bWater%w skips you across the&surface like a stone (5 hops).&%rFire/lava/water%w is fatal.",
      "Vous obtenez le %gMasque Mojo%w!&Renferme l'esprit d'une Pestoène&tombée au combat.^Équipez depuis la page des masques -&Link se transforme en petite&Pestoène légère.^%y\xA0%w attaque tournoyante (pn_attack).&Maintenez %y\xA0%w pour viser -> relâchez&pour tirer une %gbulle%w (coûte de la Magie).^Sur une %gFleur Mojo%w + %y\xA0%w pour&s'enfouir, charger et se lancer&en %gvol plané%w à distance limitée.^%bL'eau%w vous fait ricocher comme&un caillou (5 sauts). %rFeu/lave/eau%w&est fatal.",
      "Du hast die %gDeku-Maske%w!&Birgt den Geist eines gefallenen&Deku-Höriger.^Aufsetzen auf der Maskenseite -&Link verwandelt sich in einen kleinen,&leichten Deku-Höriger.^%y\xA0%w Drehangriff (pn_attack).&Halte %y\xA0%w zum Zielen -> loslassen&feuert ein %gBlasenprojektil%w (Magie).^Auf einer %gDeku-Blume%w + %y\xA0%w zum&Eingraben, Aufladen und Abschuss&in einen begrenzten %gGleitflug%w.^%bWasser%w lässt dich wie ein Stein&hüpfen (5 Sprünge). %rFeuer/Lava/Wasser%w&ist tödlich." },
    { ITEM_MM_MASK_KEATON,       PLAYER_IA_MM_MASK_KEATON,        PLAYER_MODELGROUP_DEFAULT, NEI_NO_SLOT, AGE_REQ_NONE, NULL, func_8083485C, Player_InitDefaultIA, Randomizer_DrawMmMask, RG_MM_MASK_KEATON,
      "You got the %yKeaton Mask%w!&A fox-fairy mask said to summon&the trickster Keaton.^Equip from the mask page.&%rNo gameplay effect yet%w -&currently cosmetic only.",
      "Vous obtenez le %yMasque de Keaton%w!&Un masque de renard-esprit qui&invoquerait le farceur Keaton.^Équipez depuis la page des masques.&%rPas d'effet de jeu%w -&actuellement cosmétique seulement.",
      "Du hast die %yKeaton-Maske%w!&Eine Fuchsgeist-Maske, die den&Trickser Keaton beschwören soll.^Aufsetzen auf der Maskenseite.&%rNoch kein Effekt%w -&derzeit nur Kosmetik." },
    { ITEM_MM_MASK_BREMEN,       PLAYER_IA_MM_MASK_BREMEN,        PLAYER_MODELGROUP_DEFAULT, NEI_NO_SLOT, AGE_REQ_NONE, NULL, func_8083485C, Player_InitDefaultIA, Randomizer_DrawMmMask, RG_MM_MASK_BREMEN,
      "You got the %yBremen Mask%w!&The mask of the marching musician&from the Bremen Town Musicians.^Equip from the mask page.&%rNo gameplay effect yet%w -&currently cosmetic only.",
      "Vous obtenez le %yMasque de Brême%w!&Le masque du musicien en marche&des Musiciens de Brême.^Équipez depuis la page des masques.&%rPas d'effet de jeu%w -&actuellement cosmétique seulement.",
      "Du hast die %yBremen-Maske%w!&Die Maske des marschierenden Musikers&aus den Bremer Stadtmusikanten.^Aufsetzen auf der Maskenseite.&%rNoch kein Effekt%w -&derzeit nur Kosmetik." },
    { ITEM_MM_MASK_BUNNY,        PLAYER_IA_MM_MASK_BUNNY,         PLAYER_MODELGROUP_DEFAULT, NEI_NO_SLOT, AGE_REQ_NONE, NULL, func_8083485C, Player_InitDefaultIA, Randomizer_DrawMmMask, RG_MM_MASK_BUNNY,
      "You got the %yBunny Hood%w (MM)!&The fluffy long-eared hood of&Majora's Mask.^Equip from the mask page.^Grants %gincreased run speed%w&and %ghigher jumps%w (uses the&existing Bunny Hood enhancement&while wearing the MM hood).",
      "Vous obtenez le %yMasque de Lapin%w (MM)!&La capuche aux longues oreilles&velues de Majora's Mask.^Équipez depuis la page des masques.^Octroie %gvitesse de course%w accrue&et %gsauts plus hauts%w (utilise&l'amélioration existante du Masque&de Lapin).",
      "Du hast die %yHasenohren%w (MM)!&Die flauschige lange-Ohren-Mütze&aus Majoras Mask.^Aufsetzen auf der Maskenseite.^Gewährt %gerhöhte Laufgeschwindigkeit%w&und %ghöhere Sprünge%w (nutzt die&bestehende Hasenohren-Erweiterung&beim Tragen der MM-Mütze)." },
    { ITEM_MM_MASK_DON_GERO,     PLAYER_IA_MM_MASK_DON_GERO,      PLAYER_MODELGROUP_DEFAULT, NEI_NO_SLOT, AGE_REQ_NONE, NULL, func_8083485C, Player_InitDefaultIA, Randomizer_DrawMmMask, RG_MM_MASK_DON_GERO,
      "You got %yDon Gero's Mask%w!&The conductor's mask of the&frog choir.^Equip from the mask page.&Approach the %glog at Zora's River%w&and press %y\xA0%w to %ccollect every&unclaimed Frog Song reward%w at once.^Frog flags 0-4 = purple rupee&each, flags 5-6 = heart piece.",
      "Vous obtenez le %yMasque de Don Gero%w!&Le masque du chef d'orchestre&du chœur des grenouilles.^Équipez depuis la page des masques.&Approchez la %gbûche à la Rivière Zora%w&et appuyez sur %y\xA0%w pour %crécupérer&toutes les récompenses non-réclamées%w.^Drapeaux 0-4 = rubis violet chacun,&drapeaux 5-6 = pièce de cœur.",
      "Du hast %yDon Geros Maske%w!&Die Dirigentenmaske des&Froschchors.^Aufsetzen auf der Maskenseite.&Geh zum %gBaumstamm an Zoras Fluss%w&und drücke %y\xA0%w um %calle ungeholten&Froschlied-Belohnungen%w zu sammeln.^Flags 0-4 = je lila Rupie,&Flags 5-6 = Herzteil." },
    { ITEM_MM_MASK_SCENTS,       PLAYER_IA_MM_MASK_SCENTS,        PLAYER_MODELGROUP_DEFAULT, NEI_NO_SLOT, AGE_REQ_NONE, NULL, func_8083485C, Player_InitDefaultIA, Randomizer_DrawMmMask, RG_MM_MASK_SCENTS,
      "You got the %yMask of Scents%w!&A mask said to grant the keen&nose of a beast.^Equip from the mask page.&%rNo gameplay effect yet%w -&currently cosmetic only.",
      "Vous obtenez le %yMasque des Odeurs%w!&Un masque qui octroierait le&flair d'une bête sauvage.^Équipez depuis la page des masques.&%rPas d'effet de jeu%w -&actuellement cosmétique seulement.",
      "Du hast die %yGeruchsmaske%w!&Eine Maske, die den scharfen&Geruchssinn eines Tieres verleiht.^Aufsetzen auf der Maskenseite.&%rNoch kein Effekt%w -&derzeit nur Kosmetik." },
    { ITEM_MM_MASK_GORON,        PLAYER_IA_MM_MASK_GORON,         PLAYER_MODELGROUP_DEFAULT, NEI_NO_SLOT, AGE_REQ_NONE, NULL, func_8083485C, Player_InitDefaultIA, Randomizer_DrawMmMask, RG_MM_MASK_GORON,
      "You got the %rGoron Mask%w!&Holds the spirit of the fallen&Goron hero Darmani.^Equip from the mask page -&Link transforms into a heavy,&powerful Goron.^3-hit %rpunch combo%w (%y\xA0%w / %y\xA0%w / %y\xA0%w):&left fist, right fist, butt slam.&Same heavy blunt damage as the&%rMegaton Hammer%w.^Hold %y\xA3%w to %rcurl into a ball%w -&fast Goron Roll. %y\xA0%w mid-roll&to %rground pound%w (jump -> slam).^%rImmune to lava and fire%w.&%bSinks in water%w - voids out from&deep water.",
      "Vous obtenez le %rMasque de Goron%w!&Renferme l'esprit du héros&goron déchu Darmani.^Équipez depuis la page des masques -&Link se transforme en Goron&lourd et puissant.^%rCombo de 3 coups%w (%y\xA0%w / %y\xA0%w / %y\xA0%w):&poing gauche, poing droit, attaque-fesse.&Même dégâts lourds que la&%rMasse des Titans%w.^Maintenez %y\xA3%w pour vous %renrouler%w&en boule - Roulade Goron rapide.&%y\xA0%w en roulant pour un %rgroundpound%w&(saut -> impact).^%rImmunisé au feu et à la lave%w.&%bCoule dans l'eau%w - sortie forcée&en eau profonde.",
      "Du hast die %rGoronen-Maske%w!&Birgt den Geist des gefallenen&Goronen-Helden Darmani.^Aufsetzen auf der Maskenseite -&Link verwandelt sich in einen schweren,&kraftvollen Goronen.^3-Hit %rFaustkombo%w (%y\xA0%w / %y\xA0%w / %y\xA0%w):&linke Faust, rechte Faust, Sturzangriff.&Selber schwerer Schaden wie der&%rStahlhammer%w.^Halte %y\xA3%w zum %rEinrollen%w -&schneller Goronen-Roll. %y\xA0%w im Roll&für %rStampfangriff%w (Sprung -> Schlag).^%rImmun gegen Lava und Feuer%w.&%bSinkt im Wasser%w - Voids aus&tiefem Wasser." },
    { ITEM_MM_MASK_ROMANI,       PLAYER_IA_MM_MASK_ROMANI,        PLAYER_MODELGROUP_DEFAULT, NEI_NO_SLOT, AGE_REQ_NONE, NULL, func_8083485C, Player_InitDefaultIA, Randomizer_DrawMmMask, RG_MM_MASK_ROMANI,
      "You got %yRomani's Mask%w!&A young rancher's mask carrying&her trust with cattle.^Equip from the mask page.&Walk up to %gany cow%w and press %y\xA0%w -&the cow gives you milk %cdirectly%w&without needing Epona's Song.",
      "Vous obtenez le %yMasque de Romani%w!&Le masque d'une jeune fermière&qui inspire confiance au bétail.^Équipez depuis la page des masques.&Approchez %gn'importe quelle vache%w et %y\xA0%w -&elle vous donne du lait %cdirectement%w&sans la Chanson d'Épona.",
      "Du hast %yRomanis Maske%w!&Eine junge Bauernmaske die ihr&Vertrauen zu Kühen trägt.^Aufsetzen auf der Maskenseite.&Geh zu %gjeder Kuh%w und drücke %y\xA0%w -&die Kuh gibt dir Milch %cdirekt%w&ohne Eponas Lied." },
    { ITEM_MM_MASK_CIRCUS_LEADER, PLAYER_IA_MM_MASK_CIRCUS_LEADER, PLAYER_MODELGROUP_DEFAULT, NEI_NO_SLOT, AGE_REQ_NONE, NULL, func_8083485C, Player_InitDefaultIA, Randomizer_DrawMmMask, RG_MM_MASK_CIRCUS_LEADER,
      "You got the %yCircus Leader's Mask%w!&Worn, you become Ganondorf's&%cTax Collector%w. NPCs cower&and pay tribute on sight.^Talk to a minigame NPC and the&%centire minigame is skipped%w -&its reward is granted directly:^%gShooting Gallery%w (bullet bag/quiver),&%gBombchu Bowling%w (bomb bag -> heart piece),&%gIngo%w (Epona + Hyrule Field warp),&%gTalon%w (Milk Bottle, child Lon Lon),&%gAdult Malon%w (sells cow, %p100 Rupees%w),&%gHBA%w, %gFishing Pond%w, %gChest Game%w,&%gZora Diving Game%w (Silver Scale).^Repeat visits give a small bribe.&Rando-aware: delivers shuffled checks.",
      "Vous obtenez le %yMasque du Chef de Cirque%w!&Porté, vous devenez le %cCollecteur&d'Impôts%w de Ganondorf. Les PNJ&se soumettent à votre vue.^Parler à un PNJ de mini-jeu et le&%cmini-jeu entier est sauté%w -&sa récompense est donnée directement:^%gStand de Tir%w (sac à billes/carquois),&%gBombchu Bowling%w (sac de bombes -> cœur),&%gIngo%w (Épona + transition Plaine d'Hyrule),&%gTalon%w (Bouteille de Lait, Lon Lon enfant),&%gMalon adulte%w (vend vache, %p100 Rubis%w),&%gHBA%w, %gPêche%w, %gJeu de Coffres%w,&%gJeu de Plongée Zora%w (Écaille d'Argent).^Visites répétées donnent un pourboire.&Conscient du rando: livre les items shufflés.",
      "Du hast die %yZirkusleitermaske%w!&Beim Tragen wirst du zu Ganondorfs&%cSteuereintreiber%w. NPCs zahlen&Tribut bei deinem Anblick.^Mit einem Minispiel-NPC reden und das&%ggesamte Minispiel wird übersprungen%w -&Belohnung wird direkt gegeben:^%gSchießbude%w (Munitionstasche/Köcher),&%gBombchu-Bowling%w (Bombentasche -> Herzteil),&%gIngo%w (Epona + Hyrule-Feld-Warp),&%gTalon%w (Milchflasche, Kind Lon Lon),&%gErwachsene Malon%w (verkauft Kuh, %p100 Rupien%w),&%gHBA%w, %gAngelteich%w, %gKistenspiel%w,&%gZora-Tauchspiel%w (Silberschuppe).^Wiederholungsbesuche geben Bestechungsgeld.&Rando-bewusst: liefert die geshufflten Items." },
    { ITEM_MM_MASK_KAFEI,        PLAYER_IA_MM_MASK_KAFEI,         PLAYER_MODELGROUP_DEFAULT, NEI_NO_SLOT, AGE_REQ_NONE, NULL, func_8083485C, Player_InitDefaultIA, Randomizer_DrawMmMask, RG_MM_MASK_KAFEI,
      "You got %yKafei's Mask%w!&A small mask carved in the&likeness of a missing groom.^Equip from the mask page.&While worn, %y\xA0%w toggles a&%cKafei character model%w on Link&(uses the N64_Kafei pak).&Cosmetic only.",
      "Vous obtenez le %yMasque de Kafei%w!&Un petit masque sculpté à l'image&d'un fiancé disparu.^Équipez depuis la page des masques.&Pendant le port, %y\xA0%w bascule un&%cmodèle de Kafei%w sur Link&(utilise le pak N64_Kafei).&Cosmétique seulement.",
      "Du hast %yKafeis Maske%w!&Eine kleine Maske im Antlitz&eines verschwundenen Bräutigams.^Aufsetzen auf der Maskenseite.&Beim Tragen schaltet %y\xA0%w ein&%cKafei-Charaktermodell%w an Link um&(nutzt N64_Kafei pak).&Nur Kosmetik." },
    { ITEM_MM_MASK_COUPLE,       PLAYER_IA_MM_MASK_COUPLE,        PLAYER_MODELGROUP_DEFAULT, NEI_NO_SLOT, AGE_REQ_NONE, NULL, func_8083485C, Player_InitDefaultIA, Randomizer_DrawMmMask, RG_MM_MASK_COUPLE,
      "You got the %yCouple's Mask%w!&The reunion mask of two lovers&forever entwined.^Equip from the mask page.^%cPassive regen%w while worn:&%rDay%w -> %g+1 HP every 4 frames%w&(full hearts in ~32s).&%bNight%w -> %g+1 MP every 7 frames%w&(full magic in ~32s).",
      "Vous obtenez le %yMasque des Amoureux%w!&Le masque des retrouvailles de deux&amants à jamais entrelacés.^Équipez depuis la page des masques.^%cRégénération passive%w pendant le port:&%rJour%w -> %g+1 PV toutes les 4 frames%w&(cœurs pleins en ~32s).&%bNuit%w -> %g+1 PM toutes les 7 frames%w&(magie pleine en ~32s).",
      "Du hast die %yPaarmaske%w!&Die Wiedervereinigungsmaske zweier&für immer verbundener Liebender.^Aufsetzen auf der Maskenseite.^%cPassive Regeneration%w beim Tragen:&%rTag%w -> %g+1 HP alle 4 Frames%w&(volle Herzen in ~32s).&%bNacht%w -> %g+1 MP alle 7 Frames%w&(volle Magie in ~32s)." },
    { ITEM_MM_MASK_TRUTH,        PLAYER_IA_MM_MASK_TRUTH,         PLAYER_MODELGROUP_DEFAULT, NEI_NO_SLOT, AGE_REQ_NONE, NULL, func_8083485C, Player_InitDefaultIA, Randomizer_DrawMmMask, RG_MM_MASK_TRUTH,
      "You got the %yMask of Truth%w (MM)!&The all-seeing eye that hears the&voices of beasts and stones.^Equip from the mask page.&%rNo gameplay effect yet%w -&currently cosmetic only.&(OOT's vanilla Mask of Truth is&a separate item.)",
      "Vous obtenez le %yMasque de Vérité%w (MM)!&L'œil omniscient qui entend les&voix des bêtes et des pierres.^Équipez depuis la page des masques.&%rPas d'effet de jeu%w -&actuellement cosmétique seulement.&(Le Masque de Vérité OOT est&un objet distinct.)",
      "Du hast die %yMaske der Wahrheit%w (MM)!&Das allsehende Auge, das die Stimmen&der Tiere und Steine hört.^Aufsetzen auf der Maskenseite.&%rNoch kein Effekt%w -&derzeit nur Kosmetik.&(OOTs Vanilla-Maske der Wahrheit&ist ein separates Item.)" },
    { ITEM_MM_MASK_ZORA,         PLAYER_IA_MM_MASK_ZORA,          PLAYER_MODELGROUP_DEFAULT, NEI_NO_SLOT, AGE_REQ_NONE, NULL, func_8083485C, Player_InitDefaultIA, Randomizer_DrawMmMask, RG_MM_MASK_ZORA,
      "You got the %bZora Mask%w!&Holds the spirit of the fallen&Zora guitarist Mikau.^Equip from the mask page -&Link transforms into a Zora,&master of the waters.^Real %bZora swim mechanics%w 1:1:&surface walk, %bfast dolphin swim%w,&%bswim dash%w (%y\xA0%w), %bdolphin jump%w arc.^On land: %y\xA0%w throws %bBoomerang Fins%w&(twin fin projectiles).&%y\xA3%w + %y\xA0%w raises an %cElectric Barrier%w&that shocks attackers (costs Magic).^Aerial %y\xA0%w -> flying %bjump kick%w.",
      "Vous obtenez le %bMasque de Zora%w!&Renferme l'esprit du guitariste&zora déchu Mikau.^Équipez depuis la page des masques -&Link se transforme en Zora,&maître des eaux.^Vraies %bmécaniques de nage Zora%w 1:1:&marche en surface, %bnage dauphin rapide%w,&%bdash de nage%w (%y\xA0%w), %bsaut de dauphin%w.^À terre: %y\xA0%w lance des %bAilerons Boomerang%w&(deux projectiles).&%y\xA3%w + %y\xA0%w élève une %cBarrière Électrique%w&qui foudroie les attaquants (Magie).^En l'air %y\xA0%w -> %bcoup de pied%w volant.",
      "Du hast die %bZora-Maske%w!&Birgt den Geist des gefallenen&Zora-Gitarristen Mikau.^Aufsetzen auf der Maskenseite -&Link verwandelt sich in einen Zora,&Meister des Wassers.^Echte %bZora-Schwimmmechanik%w 1:1:&Wasserlauf, %bschnelles Delfinschwimmen%w,&%bSchwimm-Dash%w (%y\xA0%w), %bDelfinsprung%w-Bogen.^An Land: %y\xA0%w wirft %bBumerang-Flossen%w&(zwei Flossen-Projektile).&%y\xA3%w + %y\xA0%w erhebt eine %cElektrische Barriere%w&die Angreifer schockt (Magie).^In der Luft %y\xA0%w -> fliegender %bSprungkick%w." },
    { ITEM_MM_MASK_KAMARO,       PLAYER_IA_MM_MASK_KAMARO,        PLAYER_MODELGROUP_DEFAULT, NEI_NO_SLOT, AGE_REQ_NONE, NULL, func_8083485C, Player_InitDefaultIA, Randomizer_DrawMmMask, RG_MM_MASK_KAMARO,
      "You got %yKamaro's Mask%w!&The mask of a wandering ghost&dancer who lost his audience.^Equip from the mask page.&%cHold %y\xA0%w to dance%w (movement locks).&Release to stop.^At %gGoron City%w near Darunia,&dance with him for ~5 seconds&to trigger %cDarunia's Joy%w reward.",
      "Vous obtenez le %yMasque de Kamaro%w!&Le masque d'un fantôme danseur&errant ayant perdu son public.^Équipez depuis la page des masques.&%cMaintenez %y\xA0%w pour danser%w (mouvement verrouillé).&Relâchez pour arrêter.^Au %gVillage Goron%w près de Darunia,&dansez avec lui ~5 secondes pour&déclencher la %cJoie de Darunia%w.",
      "Du hast %yKamaros Maske%w!&Die Maske eines umherwandernden Geist-&Tänzers ohne Publikum.^Aufsetzen auf der Maskenseite.&%cHalte %y\xA0%w zum Tanzen%w (Bewegung sperrt).&Loslassen zum Stoppen.^In %gGoronen-Stadt%w nahe Darunia,&tanze mit ihm für ~5 Sekunden,&um %cDarunias Freude%w auszulösen." },
    { ITEM_MM_MASK_GIBDO,        PLAYER_IA_MM_MASK_GIBDO,         PLAYER_MODELGROUP_DEFAULT, NEI_NO_SLOT, AGE_REQ_NONE, NULL, func_8083485C, Player_InitDefaultIA, Randomizer_DrawMmMask, RG_MM_MASK_GIBDO,
      "You got the %yGibdo Mask%w!&The decayed face of a mummy,&worn by ancient cult initiates.^Equip from the mask page.^While worn, %cReDeads and Gibdos%w&%gignore you completely%w -&they will not lunge or grab&while you wear the mask.",
      "Vous obtenez le %yMasque de Gibdo%w!&Le visage décrépit d'une momie,&porté par les initiés cultistes.^Équipez depuis la page des masques.^Pendant le port, %cReDead et Gibdo%w&%gvous ignorent complètement%w -&ils ne vous attaquent ni ne&vous attrapent.",
      "Du hast die %yGibdo-Maske%w!&Das verwitterte Antlitz einer Mumie,&getragen von Kultanwärtern.^Aufsetzen auf der Maskenseite.^Beim Tragen %gignorieren%w dich&%cReDead und Gibdo%w komplett -&sie greifen dich nicht an und&packen dich nicht." },
    { ITEM_MM_MASK_GARO,         PLAYER_IA_MM_MASK_GARO,          PLAYER_MODELGROUP_DEFAULT, NEI_NO_SLOT, AGE_REQ_NONE, NULL, func_8083485C, Player_InitDefaultIA, Randomizer_DrawMmMask, RG_MM_MASK_GARO,
      "You got %yGaro's Mask%w!&The shrouded mask of a Garo&ninja, sworn to silence.^Equip from the mask page.&%rNo gameplay effect yet%w -&currently cosmetic only.",
      "Vous obtenez le %yMasque de Garo%w!&Le masque drapé d'un ninja Garo,&qui a juré silence.^Équipez depuis la page des masques.&%rPas d'effet de jeu%w -&actuellement cosmétique seulement.",
      "Du hast %yGaros Maske%w!&Die verhüllte Maske eines Garo-&Ninjas, der Stille geschworen hat.^Aufsetzen auf der Maskenseite.&%rNoch kein Effekt%w -&derzeit nur Kosmetik." },
    { ITEM_MM_MASK_CAPTAIN,      PLAYER_IA_MM_MASK_CAPTAIN,       PLAYER_MODELGROUP_DEFAULT, NEI_NO_SLOT, AGE_REQ_NONE, NULL, func_8083485C, Player_InitDefaultIA, Randomizer_DrawMmMask, RG_MM_MASK_CAPTAIN,
      "You got the %yCaptain's Hat%w!&The crested helm of Captain&Keeta, leader of the Stalfos.^Equip from the mask page.^At night in %gHyrule Field%w only,&summons %rgiant Stalfos%w (adult Link)&or %rgiant Stalchildren%w (2x scale&and speed for child) to roam the field.&One spawns every ~5 seconds, max 3.",
      "Vous obtenez la %yCasquette du Capitaine%w!&Le casque du Capitaine Keeta,&chef des Stalfos.^Équipez depuis la page des masques.^La nuit dans la %gPlaine d'Hyrule%w,&invoque des %rStalfos géants%w (Link adulte)&ou des %rStalchild géants%w (2x taille&et vitesse pour Jeune Link).&Un toutes les ~5s, max. 3.",
      "Du hast den %yKapitänshut%w!&Der Kammhelm von Captain Keeta,&Anführer der Stalfos.^Aufsetzen auf der Maskenseite.^Nur nachts in %gHyrule-Feld%w werden&%rriesige Stalfos%w (Erwachsener) oder&%rriesige Stalchild%w (2x Größe und&Tempo für Jung) gerufen.&Einer alle ~5s, max. 3." },
    { ITEM_MM_MASK_GIANT,        PLAYER_IA_MM_MASK_GIANT,         PLAYER_MODELGROUP_DEFAULT, NEI_NO_SLOT, AGE_REQ_NONE, NULL, func_8083485C, Player_InitDefaultIA, Randomizer_DrawMmMask, RG_MM_MASK_GIANT,
      "You got the %yGiant's Mask%w!&The colossal mask said to grow&its wearer to monstrous size.^Equip from the mask page.&%rNo gameplay effect yet%w -&currently cosmetic only.&(In MM it scales Link to fight&Twinmold.)",
      "Vous obtenez le %yMasque de Géant%w!&Le masque colossal qui ferait&grandir son porteur.^Équipez depuis la page des masques.&%rPas d'effet de jeu%w -&actuellement cosmétique seulement.&(Dans MM, il agrandit Link pour&combattre Twinmold.)",
      "Du hast die %yRiesenmaske%w!&Die kolossale Maske, die ihren&Träger riesenhaft wachsen lässt.^Aufsetzen auf der Maskenseite.&%rNoch kein Effekt%w -&derzeit nur Kosmetik.&(In MM vergrößert sie Link&für den Twinmold-Kampf.)" },
    { ITEM_MM_MASK_FIERCE_DEITY, PLAYER_IA_MM_MASK_FIERCE_DEITY,  PLAYER_MODELGROUP_DEFAULT, NEI_NO_SLOT, AGE_REQ_NONE, NULL, func_8083485C, Player_InitDefaultIA, Randomizer_DrawMmMask, RG_MM_MASK_FIERCE_DEITY,
      "You got %pFierce Deity's Mask%w!&The legendary forbidden mask of&a god-like warrior.^Equip from the mask page -&Link transforms into the towering&%pFierce Deity%w. The Final Form.^%c1.5x movement speed%w - the only&form with a speed multiplier.^Wields a massive %ptwo-handed sword%w&(PLAYER_ANIMTYPE_3 stance).&Every full-health %y\xA0%w swing fires&a long-range %psword beam%w projectile.^Hyrule's strongest combat form.",
      "Vous obtenez le %pMasque du Dieu Féroce%w!&Le masque légendaire interdit d'un&guerrier divin.^Équipez depuis la page des masques -&Link se transforme en imposant&%pDieu Féroce%w. La Forme Finale.^%c1,5x vitesse de déplacement%w - la&seule forme avec un bonus de vitesse.^Manie une massive %pépée à deux mains%w&(posture PLAYER_ANIMTYPE_3).&Chaque coup %y\xA0%w à pleine santé tire&un %prayon d'épée%w à longue portée.^La forme de combat la plus puissante.",
      "Du hast %pMajoras Maske%w!&Die legendäre verbotene Maske eines&gottgleichen Kriegers.^Aufsetzen auf der Maskenseite -&Link verwandelt sich in den hoch&aufragenden %pFinsteren Gott%w. Die Endform.^%c1,5x Bewegungsgeschwindigkeit%w - die&einzige Form mit Geschwindigkeitsbonus.^Führt ein massives %pZweihandschwert%w&(PLAYER_ANIMTYPE_3-Haltung).&Jeder %y\xA0%w-Schwung bei voller Gesundheit&feuert einen %pSchwertstrahl%w in die Ferne.^Hyrules stärkste Kampfform." },
};

#define NEI_ITEMS_COUNT (sizeof(sNeiItems) / sizeof(sNeiItems[0]))

// Skijer's NEI
const NeiItem* Nei_FindByItem(int32_t item) {
    for (size_t i = 0; i < NEI_ITEMS_COUNT; i++) {
        if (sNeiItems[i].item != NEI_NO_ITEM && sNeiItems[i].item == item) {
            return &sNeiItems[i];
        }
    }
    return NULL;
}

// Skijer's NEI
const NeiItem* Nei_FindBySlot(uint8_t slot) {
    if (slot == NEI_NO_SLOT) {
        return NULL;
    }
    for (size_t i = 0; i < NEI_ITEMS_COUNT; i++) {
        if (sNeiItems[i].slot == slot) {
            return &sNeiItems[i];
        }
    }
    return NULL;
}

// Skijer's NEI
const NeiItem* Nei_FindByRg(int16_t rg) {
    if (rg == NEI_NO_RG) {
        return NULL;
    }
    for (size_t i = 0; i < NEI_ITEMS_COUNT; i++) {
        if (sNeiItems[i].rg == rg) {
            return &sNeiItems[i];
        }
    }
    return NULL;
}

static const NeiItem* ExtPlayer_FindByIA(int32_t itemAction) {
    for (size_t i = 0; i < NEI_ITEMS_COUNT; i++) {
        if (sNeiItems[i].ia == itemAction) {
            return &sNeiItems[i];
        }
    }
    return NULL;
}

/**
 * Get the PLAYER_IA_xxx value for a given ITEM_xxx value.
 */
int8_t ExtPlayer_GetItemAction(int32_t item) {
    // Handle special cases first
    if (item >= ITEM_NONE_FE) {
        return PLAYER_IA_NONE;
    }
    if (item == ITEM_LAST_USED) {
        return PLAYER_IA_SWORD_CS;
    }
    if (item == ITEM_FISHING_POLE) {
        return PLAYER_IA_FISHING_POLE;
    }

    // Vanilla-IA aliases: custom items that behave as an existing vanilla action.
    // (Their model group / update / init come from the vanilla arrays, so they are
    // intentionally NOT table rows.)
    switch (item) {
        // Bow combos and swords (originally in the expanded vanilla array).
        case ITEM_BOW_ARROW_FIRE:
            return PLAYER_IA_BOW_FIRE;
        case ITEM_BOW_ARROW_ICE:
            return PLAYER_IA_BOW_ICE;
        case ITEM_BOW_ARROW_LIGHT:
            return PLAYER_IA_BOW_LIGHT;
        case ITEM_SWORD_KOKIRI:
            return PLAYER_IA_SWORD_KOKIRI;
        case ITEM_SWORD_MASTER:
            return PLAYER_IA_SWORD_MASTER;
        case ITEM_SWORD_BGS:
            return PLAYER_IA_SWORD_BIGGORON;

        // Chateau Romani (bottle item - drink to activate infinite magic)
        case ITEM_CHATEAU_ROMANI:
            return PLAYER_IA_BOTTLE_POTION_BLUE;

        // SW97 Medallion spells (quest medallions → spell IAs)
        case ITEM_MEDALLION_FOREST:
            return PLAYER_IA_MAGIC_SPELL_15;
        case ITEM_MEDALLION_SPIRIT:
            return PLAYER_IA_MAGIC_SPELL_16;
        case ITEM_MEDALLION_SHADOW:
            return PLAYER_IA_MAGIC_SPELL_17;
        case ITEM_MEDALLION_WATER:
            return PLAYER_IA_FARORES_WIND;
        case ITEM_MEDALLION_LIGHT:
            return PLAYER_IA_NAYRUS_LOVE;
        case ITEM_MEDALLION_FIRE:
            return PLAYER_IA_DINS_FIRE;

        // SW97 Arrow items: bow IA if Sw97_PreferBow() (adult by default, or
        // child-with-bow when both BowSlingshotAmmoFix and TimelessEquipment are on),
        // slingshot IA otherwise. (Dynamic — cannot be a static table cell.)
        case ITEM_SW97_ARROW_FIRE:
            return Sw97_PreferBow() ? PLAYER_IA_BOW_FIRE : PLAYER_IA_SLINGSHOT;
        case ITEM_SW97_ARROW_ICE:
            return Sw97_PreferBow() ? PLAYER_IA_BOW_ICE : PLAYER_IA_SLINGSHOT;
        case ITEM_SW97_ARROW_LIGHT:
            return Sw97_PreferBow() ? PLAYER_IA_BOW_LIGHT : PLAYER_IA_SLINGSHOT;
        case ITEM_SW97_ARROW_DARK:
            return Sw97_PreferBow() ? PLAYER_IA_BOW_0C : PLAYER_IA_SLINGSHOT;
        case ITEM_SW97_ARROW_SOUL:
            return Sw97_PreferBow() ? PLAYER_IA_BOW_0D : PLAYER_IA_SLINGSHOT;
        case ITEM_SW97_ARROW_WIND:
            return Sw97_PreferBow() ? PLAYER_IA_BOW_0E : PLAYER_IA_SLINGSHOT;

        default:
            break;
    }

    // Custom items: unified NEI registry. Skijer's NEI
    const NeiItem* desc = Nei_FindByItem(item);
    if (desc != NULL) {
        return (int8_t)desc->ia;
    }

    // For vanilla items, use the original array if within bounds
    if (item < VANILLA_SITEMACTIONS_SIZE) {
        return sItemActions[item];
    }

    // For items in the gap (equipment, songs, quest items, etc.), return NONE
    return PLAYER_IA_NONE;
}

/**
 * Get the model group for a given PLAYER_IA_xxx value.
 */
uint8_t ExtPlayer_GetActionModelGroup(int32_t itemAction) {
    const NeiItem* desc = ExtPlayer_FindByIA(itemAction);
    if (desc != NULL) {
        return desc->modelGroup;
    }

    // For vanilla item actions, use the original array if within bounds
    if (itemAction < VANILLA_PLAYER_IA_COUNT) {
        return sActionModelGroups[itemAction];
    }

    return PLAYER_MODELGROUP_DEFAULT;
}

/**
 * Get the update function for a given PLAYER_IA_xxx value.
 */
ItemActionUpdateFunc ExtPlayer_GetItemActionUpdateFunc(int32_t itemAction) {
    const NeiItem* desc = ExtPlayer_FindByIA(itemAction);
    if (desc != NULL) {
        return desc->updateFn;
    }

    // For vanilla item actions, use the original array if within bounds
    if (itemAction < VANILLA_PLAYER_IA_COUNT) {
        return sItemActionUpdateFuncs[itemAction];
    }

    return func_8083485C;
}

/**
 * Get the init function for a given PLAYER_IA_xxx value.
 */
ItemActionInitFunc ExtPlayer_GetItemActionInitFunc(int32_t itemAction) {
    const NeiItem* desc = ExtPlayer_FindByIA(itemAction);
    if (desc != NULL) {
        return desc->initFn;
    }

    // For vanilla item actions, use the original array if within bounds
    if (itemAction < VANILLA_PLAYER_IA_COUNT) {
        return sItemActionInitFuncs[itemAction];
    }

    return Player_InitDefaultIA;
}
