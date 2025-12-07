from pathlib import Path
import sys, os, time, secrets, base64, textwrap, subprocess, random, ast, lzma, zlib
from Crypto.Cipher import AES

CHUNK_MIN=1
CHUNK_MAX=4
NONCE_LEN=12
SALT_LEN=32

def aes_enc(data,key):
    n=secrets.token_bytes(NONCE_LEN)
    c=AES.new(key,AES.MODE_GCM,nonce=n)
    ct,tag=c.encrypt_and_digest(data)
    return n+tag+ct

class Mut(ast.NodeTransformer):
    def visit_FunctionDef(self,node):
        self.generic_visit(node)
        node.body.insert(0,ast.Expr(value=ast.Constant(value="x"+str(random.randint(1000,9999)))))
        return node

def mutate(src):
    t=ast.parse(src)
    t=Mut().visit(t)
    ast.fix_missing_locations(t)
    return ast.unparse(t)

def chunks(data,n,r):
    if n<=1:return[data]
    pts=sorted(r.sample(range(1,len(data)),n-1))
    out=[];p=0
    for x in pts:
        out.append(data[p:x]);p=x
    out.append(data[p:])
    return out

def loader(enc,salt):
    return f"""
import base64,lzma,zlib
import keymodule
from Crypto.Cipher import AES
salt=base64.b64decode({repr(salt)})
chunks={repr(enc)}
k=keymodule.derive_key(salt)
buf=bytearray()
for c in chunks:
    r=base64.b64decode(c)
    n=r[:12];t=r[12:28];ct=r[28:]
    pt=AES.new(k,AES.MODE_GCM,nonce=n).decrypt_and_verify(ct,t)
    buf.extend(pt)
d=lzma.decompress(zlib.decompress(bytes(buf)))
s=d.decode('utf-8')
exec(compile(s,"<x>","exec"))
"""

def build_keymodule(path):
    subprocess.check_call([sys.executable,"setup.py","build_ext","--inplace"],cwd=str(path))

def obfuscate(fp):
    p=Path(fp)
    src=p.read_text()
    src=mutate(src)
    comp=zlib.compress(lzma.compress(src.encode()))
    salt=secrets.token_bytes(SALT_LEN)
    try:
        import keymodule
    except:
        build_keymodule(p.parent)
        import importlib;importlib.invalidate_caches()
        import keymodule
    key=keymodule.derive_key(salt)
    r=random.Random(time.time())
    parts=r.randint(CHUNK_MIN,CHUNK_MAX)
    pl=chunks(comp,parts,r)
    enc=[base64.b64encode(aes_enc(x,key)).decode() for x in pl]
    out=p.with_suffix(".cloud.obf.py")
    out.write_text(loader(enc,base64.b64encode(salt).decode()))
    print("OK:",out)

def main():
    if len(sys.argv)<2:
        print("usage: python cloud_obfuscator.py <file.py>")
        sys.exit(1)
    obfuscate(sys.argv[1])

if __name__=="__main__":
    main()