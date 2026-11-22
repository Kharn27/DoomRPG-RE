#!/usr/bin/env python3
"""Temporary read-only QA of user-supplied OPL WAVs on GitHub Actions.

Compiles the exact production IMA-ADPCM parser from esp_audio_entrance_adpcm.h.
Decodes via ffmpeg to assess signal statistics, WITHOUT modifying WAV files.
Quiet-window measurements are estimates, NOT proof of hiss or clean listening.
"""
from array import array
from pathlib import Path
import math
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[2]
MUSIC = ROOT / "sdroot/doomrpg/music/opl"
SAMPLE_RATE = 22050
WINDOW_SAMPLES = 2205

CHECK = r"""
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include "esp_audio_entrance_adpcm.h"
int main(int argc,char** argv) {
    if (argc != 2) return 2;
    FILE* file=fopen(argv[1],"rb");
    if (!file) { puts("HEADER_FAIL open"); return 2; }
    if (fseek(file,0,SEEK_END)!=0) return 2;
    long length=ftell(file);
    if(length<=0 || (unsigned long)length>UINT32_MAX) return 2;
    rewind(file);
    uint8_t buf[512]={0};
    uint32_t n=(uint32_t)length<512U?(uint32_t)length:512U;
    if(fread(buf,1,n,file)!=n) return 2;
    fclose(file);
    EspEntranceAdpcmWav wav={0};
    if(!EspEntranceAdpcm_parse(buf,n,(uint32_t)length,&wav)){
        puts("HEADER_FAIL production_ima_parser");return 2;
    }
    printf("HEADER_OK frames=%u duration=%.3fs align=%u samplesPerBlock=%u dataBytes=%u dataOffset=%u\\n",
        wav.sampleFrames,(double)wav.sampleFrames/22050.0,
        wav.blockAlign,wav.samplesPerBlock,wav.dataBytes,wav.dataOffset);
    return 0;
}
"""

def dbfs(squares, count):
    if count == 0 or squares <= 0:
        return "-inf"
    return f"{20.0*math.log10(math.sqrt(squares/count)/32768.0):.1f}"

def inspect(path, binary):
    result=subprocess.run([str(binary),str(path)],capture_output=True,text=True)
    print(" ",result.stdout.strip() or result.stderr.strip(),flush=True)
    if result.returncode: return False
    cmd=["ffmpeg","-nostdin","-hide_banner","-loglevel","error",
         "-i",str(path),"-map","0:a:0","-ac","1","-ar",str(SAMPLE_RATE),
         "-f","s16le","-acodec","pcm_s16le","-"]
    proc=subprocess.Popen(cmd,stdout=subprocess.PIPE,stderr=subprocess.PIPE)
    n=0; total=0; sum_sq=0; high_peak=0; clipping=0; jumps=0
    prev=None; floor=(float("inf"),None); window=[]
    remain=b""
    while True:
        chunk=proc.stdout.read(65536)
        if not chunk: break
        chunk=remain+chunk
        if len(chunk)&1: remain=chunk[-1:];chunk=chunk[:-1]
        else: remain=b""
        samples=array("h")
        samples.frombytes(chunk)
        if sys.byteorder!="little":samples.byteswap()
        for s in samples:
            n+=1; total+=s; sum_sq+=s*s
            if abs(s)>high_peak: high_peak=abs(s)
            if abs(s)>=32760:clipping+=1
            if prev is not None and abs(s-prev)>=24000:jumps+=1
            prev=s
            window.append(s)
            if len(window)>=WINDOW_SAMPLES:
                energy=sum(v*v for v in window)/WINDOW_SAMPLES
                if energy<floor[0]:floor=(energy,list(window))
                window.clear()
    stderr=proc.stderr.read().decode("utf-8",errors="replace")
    code=proc.wait()
    if code or remain or not n:
        print("  DECODE_FAIL",code,stderr[:300],flush=True)
        return False
    avg=total/n
    hf_db="n/a";quiet_db="n/a"
    if floor[1]:
        q=floor[1]
        quiet_db=dbfs(sum(s*s for s in q),len(q))
        # Main-task-only analysis: low-pass residual is HF ENERGY in the
        # quietest 100ms, NOT conclusive audible noise/hiss classification.
        lp=0.0;residual=0.0
        for s in q:
            lp+=0.35*(s-lp)
            residual+=(s-lp)*(s-lp)
        hf_db=dbfs(residual,len(q))
    print(f"  PCM frames={n} rms={dbfs(sum_sq,n)}dBFS peak={high_peak}/32768"
          f" DC={avg/32768.0*100:.3f}% clipped={clipping} jumps>=24000={jumps}"
          f" quietest100ms={quiet_db}dBFS quietHF={hf_db}dBFS",flush=True)
    return True

def main():
    wavs=sorted(MUSIC.glob("*.wav"))
    print("WAV-QC count",len(wavs),"source",MUSIC,flush=True)
    assert len(wavs)==15, "expected user's 15 added tracks"
    with tempfile.TemporaryDirectory(prefix="doom-s1-wav-qa-") as d:
        check=Path(d)/"check.c"; exe=Path(d)/"check"
        check.write_text(CHECK)
        subprocess.run(["cc","-std=c11","-Wall","-Wextra","-Werror","-pedantic",
            "-I",str(ROOT/"ESP32/src"),str(check),"-o",str(exe)],check=True)
        failures=0
        for f in wavs:
            print("WAV-QC",f.name,f.stat().st_size,"bytes",flush=True)
            if not inspect(f,exe):failures+=1
    print("WAV-QC SUMMARY files",len(wavs),"failures",failures,flush=True)
    return 1 if failures else 0

if __name__=="__main__":
    raise SystemExit(main())
