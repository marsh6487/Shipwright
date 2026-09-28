"""Constrain this visual revision to the approved source diff, including RNG cadence."""
from pathlib import Path
import re, subprocess, sys
ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'scripts/diagnostics'))
from run_time_pedestal_tests import functions
BASE='cec63fce86b1f582f6e61ad6a98eca6c3cca784b'
def baseline(p):return subprocess.check_output(['git','show',f'{BASE}:{p}'],cwd=ROOT,text=True)
def tokens(s):return re.sub(r'\s+','',re.sub(r'//[^\n]*|/\*.*?\*/','',s,flags=re.S))
def gameplay(s):
 s=re.sub(r'\s*(?:FX_DrawChargeAura|FX_DrawSpinFireCylinder|NeiUsedMagic_DrawCharge|NeiUsedMagic_DrawSpin)\([^;]*;', '',s)
 s=s.replace('RodCommon_PreserveChargeSparkCadence','FX_SpawnRodSwingParticles')
 s=re.sub(r'    // Use bright yellow.*?    if \(\(play->gameplayFrames % 3\)',
          '    if ((play->gameplayFrames % 3)',s,flags=re.S)
 s=re.sub(r'        // Sample the existing wave/beam state;[^\n]*\n        for \(s32 i = 0; i < .*?\n        }\n','',s,flags=re.S)
 return tokens(s)
for element in ('fire','ice','light'):
 path=f'soh/mods/items/logic/item_rod_{element}.c'
 old=functions(baseline(path))
 # Separately tested put-away audio fix is now part of this combined revision.
 # Normalize only the exact per-rod ownership wrappers, retaining gameplay checks.
 current=(ROOT/path).read_text()
 for kind in ('Equip','Unequip'):
  current=current.replace(f'ItemEquip_Play{kind}SFXForAction(play, p, PLAYER_IA_ROD_{element.upper()})',
                          f'ItemEquip_Play{kind}SFX(play, p)')
 new=functions(current)
 assert old.keys()==new.keys(),(element,'unexpected function additions/deletions')
 for name in old:
  assert gameplay(old[name])==gameplay(new[name]),(element,name,'nonvisual logic changed')
# The invisible original particle still occupies the same effect slot for the
# same life, running the real unmodified GSpk update (two RNG draws per tick).
old=functions((ROOT/'soh/mods/items/helpers/fx_helper.c').read_text())['FX_SpawnRodSwingParticles']
new=functions((ROOT/'soh/mods/items/logic/item_rod_common.c').read_text())['RodCommon_PreserveChargeSparkCadence']
old=old.replace('FX_SpawnRodSwingParticles','RodCommon_PreserveChargeSparkCadence').replace('&env, 100, 10','&env, 0, 0')
assert tokens(old)==tokens(new)
for path in ('soh/src/overlays/effects/ovl_Effect_Ss_G_Spk/z_eff_ss_g_spk.c',
             'soh/mods/items/logic/item_time_gate.c'):
 assert (ROOT/path).read_text()==baseline(path),path
# Fire projectile draw tail is deliberately byte-for-byte the accepted baseline.
path='soh/mods/items/objects/object_firerod.c';marker='    // Draw active fireball sets.'
assert baseline(path).split(marker,1)[1]==(ROOT/path).read_text().split(marker,1)[1]
path='soh/mods/items/objects/object_lightrod.c'
assert (ROOT/path).read_text().count('Rand_ZeroOne()')==baseline(path).count('Rand_ZeroOne()')==1
path='soh/soh/Enhancements/randomizer/NeiGiPresentation.cpp'
old=functions(baseline(path))['NeiGi_DrawMesh']
new=functions((ROOT/path).read_text())['NeiGi_DrawMeshMaterial']
new=re.sub(r'static void NeiGi_DrawMeshMaterial\(.*?\) \{',
           'void NeiGi_DrawMesh(PlayState* play, const NeiGi::Mesh& mesh, Kind orb) {',new,count=1,flags=re.S)
new=new.replace('(material ? 32 : 63)','63')
a=new.index('    if (material) {');b=new.index('    } else if (orb != Kind::Neutral) {',a)
new=new[:a]+'    if (orb != Kind::Neutral) {'+new[b+len('    } else if (orb != Kind::Neutral) {'):]
assert tokens(old)==tokens(new),'Existing GI batcher command path changed'
print('USED VFX source contract: rod gameplay, Time Gate state, Fire shots, native particles and RNG cadence unchanged')
