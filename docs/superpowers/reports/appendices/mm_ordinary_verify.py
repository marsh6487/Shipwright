#!/usr/bin/env python3
"""Run exact production-function resource/lifecycle CPU fixtures, with normal build headers.
Usage: source build-env.sh; python mm_ordinary_verify.py BUILD_DIR MM_ARCHIVE
No archive writes. Temporary objects live outside the build tree; no concurrent Ninja writer.
"""
from pathlib import Path
import os, re, shlex, subprocess, sys, tempfile
ROOT = Path(__file__).resolve().parents[4]
BUILD = Path(sys.argv[1]).resolve()
ARCHIVE = Path(sys.argv[2]).resolve()
WORK = Path(tempfile.mkdtemp(prefix='mm-ordinary-'))
print('Fixture output:', WORK, flush=True)

def function(source, name):
    # Extract a definition, preserving its actual body; no implementation is mirrored in the fixture.
    match = re.search(r'^[^\n;{}]*\b' + re.escape(name) + r'\([^;{}]*\)\s*\{', source, re.M)
    if not match: raise RuntimeError('Missing production definition: ' + name)
    start, depth, i = match.start(), 1, match.end()
    while depth:
        if source[i] == '{': depth += 1
        elif source[i] == '}': depth -= 1
        i += 1
    return source[start:i] + '\n'

ninja = (BUILD/'build.ninja').read_text()
def flags(object_suffix):
    match = re.search(r'^build soh/CMakeFiles/soh.dir/' + re.escape(object_suffix) + r':[^\n]*\n((?:  [^\n]*\n)+)', ninja, re.M)
    if not match: raise RuntimeError(object_suffix)
    return sum((shlex.split(v) for k,v in re.findall(r'^  (DEFINES|FLAGS|INCLUDES) = (.*)$', match[1], re.M)), [])
CFLAGS = flags('src/overlays/actors/ovl_En_Viewer/z_en_viewer.c.o') + ['-ffunction-sections', '-fdata-sections', '-DNDEBUG']
CPPFLAGS = flags('mods/transformation_masks/assets/mm_asset_loader.cpp.o') + ['-ffunction-sections', '-fdata-sections', '-DNDEBUG']
def run(args): subprocess.run([str(a) for a in args], cwd=ROOT, check=True)
def compile(path, cpp=False):
    out=WORK/(path.name+'.o')
    run(['c++' if cpp else 'cc', *(CPPFLAGS if cpp else CFLAGS), '-I'+str(WORK), '-c', path, '-o', out])
    return out

if os.environ.get("MM_VERIFY_STAGE") != "viewer":
    loader = (ROOT/'soh/mods/transformation_masks/assets/mm_asset_loader.cpp').read_text()
    resource_source = (ROOT/'soh/tests/static_story_mm_resource_test.cpp').read_text()
    resource_source=resource_source.replace('/* PRODUCTION_RESOURCE_FUNCTIONS */', '\n'.join(function(loader,n) for n in [
        'MmAssets_LoadResourceObjectFromMmArchive', 'MmAssets_LoadNormalActor', 'MmAssets_LoadKafei', 'MmAssets_ReleaseNormalActor']))
    p=WORK/'resource_fixture.cpp';p.write_text(resource_source)
    objects=[compile(p,True)]
    for name in ['Skeleton','SkeletonLimb','Animation','PlayerAnimation']:
        objects += [compile(ROOT/f'soh/soh/resource/type/{name}.cpp',True),compile(ROOT/f'soh/soh/resource/importer/{name}Factory.cpp',True)]
    objects += [compile(ROOT/'soh/src/overlays/actors/ovl_En_Viewer/static_story_mm_actor.c')]
    libs = ['libultraship/src/libultraship.a','_deps/stormlib-build/libstorm.a','libultraship/libImGui.a',
            'libultraship/libstb.a','libultraship/liblibgfxd.a','_deps/prism-build/libprism.a','libultraship/libmonocypher.a']
    sysroot=ROOT.parent/'linux-sysroot/usr/lib/x86_64-linux-gnu'
    run(['c++',*objects,'-Wl,--gc-sections','-Wl,--start-group',*[BUILD/l for l in libs],'-Wl,--end-group',
         '-L'+str(sysroot),'-Wl,-rpath,'+str(sysroot),'-lSDL2','-lOpenGL','-lzip','-ltinyxml2','-lspdlog','-lfmt','-lpng','-lz','-ldl','-pthread','-o',WORK/'resource_fixture'])
    run([WORK/'resource_fixture',ARCHIVE])


if os.environ.get("MM_VERIFY_STAGE") != "resource":
    viewer=(ROOT/'soh/src/overlays/actors/ovl_En_Viewer/z_en_viewer.c').read_text()
    fixture=(ROOT/'soh/tests/static_story_mm_viewer_test.c').read_text()
    loader=(ROOT/'soh/mods/transformation_masks/assets/mm_asset_loader.cpp').read_text()
    table=re.search(r'static Gfx sMmOpaqueRenderModeDL\[\] = \{.*?\n\};',loader,re.S).group(0)
    fixture=fixture.replace('/* PRODUCTION_OPAQUE_RENDER_MODE */',table+'\n'+function(loader,'MmAssets_GetOpaqueRenderMode'))
    functions=['EnViewer_Update','EnViewer_Destroy','EnViewerStatic_WaitForObjects','EnViewerStatic_Update',
               'EnViewer_StaticTreasureChestShopGalOverrideLimbDraw','EnViewer_StaticOrdinaryMmOverrideLimbDraw','EnViewer_DrawStaticMmActor','EnViewer_DrawStaticSkullKid']
    fixture=fixture.replace('/* PRODUCTION_VIEWER_FUNCTIONS */','\n'.join(function(viewer,n) for n in functions))
    p=WORK/'viewer_fixture.c';p.write_text(fixture)
    objects=[compile(p)]
    for name in ['actor','mm_actor','ganon']:
        objects.append(compile(ROOT/f'soh/src/overlays/actors/ovl_En_Viewer/static_story_{name}.c'))
    run(['cc',*objects,'-Wl,--gc-sections','-lm','-o',WORK/'viewer_fixture'])
    run([WORK/'viewer_fixture'])
