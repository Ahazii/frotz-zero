#!/usr/bin/env python3
"""Build CC0 test stories and original indexed graphics/audio. No private inputs."""
import pathlib,subprocess,struct,math,zlib
root=pathlib.Path(__file__).resolve().parents[1];out=root/'build/fixtures';out.mkdir(parents=True,exist_ok=True)
for v in (3,4,5,7,8):subprocess.run(['inform6',f'-v{v}',str(root/'tests/fixtures/lantern.inf'),str(out/f'lantern.z{v}')],check=True)
subprocess.run(['inform6','-v6',str(root/'tests/fixtures/graphics.inf'),str(out/'graphics.z6')],check=True)
subprocess.run(['inform6','-v5',str(root/'tests/fixtures/audio.inf'),str(out/'audio.z5')],check=True)
for name,version in [('intro',4),('framed',6),('automatic',6),('pagination',3),('text-modes',5),('text-modes',7),('text-modes',8)]:
    subprocess.run(['inform6',f'-v{version}',str(root/f'tests/fixtures/{name}.inf'),str(out/f'{name}.z{version}')],check=True)
def chunk(name,data):return name+struct.pack('>I',len(data))+data+(b'\0' if len(data)%2 else b'')
def png_chunk(name,data):return struct.pack('>I',len(data))+name+data+struct.pack('>I',zlib.crc32(name+data)&0xffffffff)
width,height=160,80
pixels=bytearray()
for y in range(height):
    pixels.append(0)
    for x in range(width):pixels.extend((50 if (x//10+y//10)%2 else 220,100+x//2,60+y*2))
png=b'\x89PNG\r\n\x1a\n'+png_chunk(b'IHDR',struct.pack('>IIBBBBB',width,height,8,2,0,0,0))+png_chunk(b'IDAT',zlib.compress(pixels))+png_chunk(b'IEND',b'')
# 80-bit extended float 44100 = 0x400e ac44000000000000.
rate=44100;samples=rate//3;pcm=b''.join(struct.pack('>h',int(6000*math.sin(2*math.pi*440*i/rate))) for i in range(samples))
aiff=b'AIFF'+chunk(b'COMM',struct.pack('>hIh',1,samples,16)+bytes.fromhex('400eac44000000000000'))+chunk(b'SSND',b'\0'*8+pcm)
full_aiff=b'FORM'+struct.pack('>I',len(aiff))+aiff
(out/'tone.aiff').write_bytes(full_aiff)
for name,parts in [
    ('graphics',[(b'Exec',0,b'ZCOD',(out/'graphics.z6').read_bytes()),(b'Pict',1,b'PNG ',png),(b'Snd ',3,b'FORM',aiff)]),
    ('automatic',[(b'Exec',0,b'ZCOD',(out/'automatic.z6').read_bytes()),(b'Pict',1,b'PNG ',png)]),
    ('audio',[(b'Exec',0,b'ZCOD',(out/'audio.z5').read_bytes()),(b'Snd ',3,b'FORM',aiff)]),
]:
    offset=12+8+4+12*len(parts);index=struct.pack('>I',len(parts));payload=b''
    for usage,number,kind,data in parts:
        index+=usage+struct.pack('>II',number,offset);part=chunk(kind,data);payload+=part;offset+=len(part)
    body=b'IFRS'+chunk(b'RIdx',index)+payload
    (out/(name+'.blb')).write_bytes(b'FORM'+struct.pack('>I',len(body))+body)
print('Built public fixtures in',out)
