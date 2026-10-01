import argparse, hashlib, json, os, subprocess, time, zipfile
from timing_replay_summary import postprocess, persist
from pathlib import Path

parser=argparse.ArgumentParser()
parser.add_argument('mode',choices=['span-crt','separate-crt','span-none','separate-none'])
parser.add_argument('--archive',type=Path,required=True)
parser.add_argument('--output',type=Path,required=True)
parser.add_argument('--executable',type=Path)
args=parser.parse_args()
workspace=Path(__file__).resolve().parents[2]
candidate=workspace/'fzero-unified-candidate'
source=Path('E:/Source/fzero-triple-wheel-integration/build-integration/rig-preview')
case=json.loads((source/'wheel-drive-20260928-235320.case.json').read_text())
hashfile=lambda p: hashlib.sha256(p.read_bytes()).hexdigest()
for kind,path in [('rom',source/'fzero.sfc'),('config',source/'diagnostics/wheel-drive-20260928-235320.original-config.ini'),('video',source/'diagnostics/wheel-drive-20260928-235320.original-video.ini'),('msuPatch',source/'music/F-Zero DX Expanded (JUD6MENT)/f-zero_msu1_stock.ips')]:
    assert hashfile(path)==case['artifacts'][kind]['sha256'],kind
record=source/case['source']['path']
assert hashfile(record)==case['source']['sha256']
assert hashfile(source/case['initialState']['path'])==case['initialState']['sha256']
folder=args.output.resolve()
folder.mkdir(parents=True,exist_ok=False)
archive=args.archive.resolve()
with zipfile.ZipFile(archive) as z:
    manifest=json.loads(z.read('manifest.json'))
    for name,expected in manifest['files'].items():
        assert hashlib.sha256(z.read(name)).hexdigest()==expected,name
    z.extractall(folder)
if args.executable:
    (folder/'FZeroSNESRecomp.exe').write_bytes(args.executable.read_bytes())
pack=source/'music/F-Zero DX Expanded (JUD6MENT)'
(folder/'config.ini').write_text('[Graphics]\nFullscreen=1\nLinearFiltering=0\nShader='+('assets/shaders/crt-soft.glslp' if args.mode.endswith('crt') else '')+'\n[Sound]\nAudioFreq=32040\nVolume=0\nMsu1Enabled=1\nMsu1Dir='+pack.as_posix()+'\n[ForceFeedback]\nEnabled=0\n[Telemetry]\nEnabled=0\n[Rewind]\nEnabled=0\n')
video=(source/'diagnostics/wheel-drive-20260928-235320.original-video.ini').read_text().replace('PresentationFPS=0','PresentationFPS=60').replace('Diagnostics=0','Diagnostics=1')
video+='\nTripleOutputMode='+('Separate' if args.mode.startswith('separate') else 'Span')+'\nTriplePanelWidthMm=708\nTripleEyeDistanceMm=660\nTripleEyeHeightMm=0\nTripleLeftAngleDeg=70\nTripleRightAngleDeg=70\nTripleBezelGapMm=8\n'
(folder/'fzero-video.ini').write_text(video)
env={k:v for k,v in os.environ.items() if not k.upper().startswith(('FZERO_','SNESRECOMP_','SDL_'))}
env.update(FZERO_REPLAY_PLAYTHROUGH=str(record),SDL_AUDIODRIVER='dummy',SNESRECOMP_MSU1=str(pack),SNESRECOMP_FRAME_BMP=str(folder/'final-source-frame.bmp'),SNESRECOMP_SAVE_ROOT=str(Path(os.environ['TEMP'])/('fz-timing-'+str(os.getpid()))))
if args.mode.startswith('separate'):env['FZERO_TRIPLE_SPLIT_SPAN_TEST']='1'
started=time.monotonic()
with (folder/'stdout.log').open('w') as stdout,(folder/'stderr.log').open('w') as stderr:
    try:
        run=subprocess.run([str(folder/'FZeroSNESRecomp.exe'),str(source/'fzero.sfc')],cwd=folder,env=env,stdout=stdout,stderr=stderr,timeout=300)
        returncode=run.returncode
    except subprocess.TimeoutExpired:
        returncode=124
text=(folder/'stderr.log').read_text(errors='replace')
result={'mode':args.mode,'returncode':returncode,'elapsedSeconds':round(time.monotonic()-started,3),'executableSha256':hashfile(folder/'FZeroSNESRecomp.exe'),'recordingSha256':hashfile(record),'physicalFfb':False,'initialStateSha256':hashfile(source/case['initialState']['path']),'caseSha256':hashfile(source/'wheel-drive-20260928-235320.case.json'),'packageSha256':hashfile(archive),'displayMode':'one 7680x1440 Surround display','separateModeIsDiagnostic':args.mode.startswith('separate'),'complete':'visual replay complete' in text,'ffbInitializationObserved':'[fzero-ffb] active' in text}
result.pop('complete', None)
try:
    result.update(postprocess(folder,text,returncode))
except Exception as error:
    result.update(validated=False, errors=['Postprocessing failed: '+type(error).__name__+': '+str(error)])
persist(folder/'result.json', result)
print(json.dumps(result))
raise SystemExit(0 if result['validated'] else 1)
