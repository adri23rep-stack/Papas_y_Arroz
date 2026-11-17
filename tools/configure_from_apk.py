#!/usr/bin/env python3
"""Extract this APK's portal configuration locally; never contacts its servers."""
import argparse
import json
import re
import uuid
from pathlib import Path

def android_base64(value):
    alphabet='ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/'
    value=value.replace('\r','').replace('\n','');out=bytearray()
    for offset in range(0,len(value),4):
        block=value[offset:offset+4]
        if len(block)<2:break
        v=[alphabet.find(c) for c in block]+[-1]*4
        out.append(((v[0]<<2)&252)|((v[1]>>4)&3))
        if len(block)>2 and block[2]!='=':out.append(((v[1]<<4)&240)|((v[2]>>2)&15))
        if len(block)>3 and block[3]!='=':out.append(((v[2]<<6)&192)|(v[3]&63))
    return bytes(out)

def decrypt(value,key):
    from cryptography.hazmat.primitives.ciphers import Cipher, modes
    from cryptography.hazmat.primitives.padding import PKCS7
    try:
        from cryptography.hazmat.decrepit.ciphers.algorithms import TripleDES
    except ImportError:
        from cryptography.hazmat.primitives.ciphers.algorithms import TripleDES
    cipher=android_base64(bytes.fromhex(value).decode('ascii'))
    decoded=android_base64(key)
    if len(decoded)<24:raise ValueError('Invalid extracted key')
    dec=Cipher(TripleDES(decoded[:24]),modes.ECB()).decryptor()
    padded=dec.update(cipher)+dec.finalize();unpad=PKCS7(64).unpadder()
    return (unpad.update(padded)+unpad.finalize()).decode('utf-8')

def configure(apk_path,output,platform='switch'):
    # A newer APK can move the portal crypto material to another class.
    # Candidate selection is validated by decrypting BOTH hostname resources
    # and the manifest value before any private configuration is written.
    from loguru import logger
    logger.remove()
    from androguard.core.apk import APK
    from androguard.core.dex import DEX
    apk=APK(str(apk_path));root=None;root_candidates=[];app_id=None;salt=None;aes_key=None;api_version=None
    for raw in apk.get_all_dex():
        dex=DEX(raw)
        for cls in dex.get_classes():
            wanted={'Lk2/b;':{'g','i'},'Lj7/a;':{'i'},'Lk2/a;':{'b'},'Lo7/a;':{'i'},'Lib/c;':{'c'}}.get(cls.get_name())
            if wanted is None:continue
            for method in cls.get_methods():
                if method.get_name() not in wanted or not method.get_code():continue
                if cls.get_name()=='Lo7/a;':
                    values=[v for i in method.get_instructions() if i.get_name().startswith('const') and hasattr(i,'get_literals') for v in i.get_literals()]
                    api_version=next((v for v in values if isinstance(v,int) and v>10000),None)
                    continue
                strings=[i.get_string() for i in method.get_instructions() if i.get_name().startswith('const-string')]
                if cls.get_name()=='Lk2/b;' and method.get_name()=='g':
                    root=next((s for s in strings if len(s)>24),None)
                elif cls.get_name()=='Lib/c;':
                    root_candidates.extend(s for s in strings if 24<=len(s)<=64)
                elif cls.get_name()=='Lk2/b;':
                    salt=next((s for s in strings if len(s)>24),None)
                elif cls.get_name()=='Lk2/a;':aes_key=next((s for s in strings if len(s)==16),None)
                else:app_id=next((s for s in strings if s.startswith('com.')),None)
    # APK obfuscation can move the hardcoded API version; use the signed
    # manifest versionCode as a fallback. Tested against Xuper 4.34.4 (43404).
    manifest_version=apk.get_androidversion_code()
    if not api_version and str(manifest_version).isdigit() and 10000 <= int(manifest_version) <= 99999999:
        api_version=int(manifest_version)
    if not (root or root_candidates) or not app_id or not salt or not aes_key or not api_version:
        raise ValueError('Unsupported APK: expected portal configuration was not found')
    ns='{http://schemas.android.com/apk/res/android}'
    meta=next((x.get(ns+'value') for x in apk.get_android_manifest_xml().findall('.//meta-data')
               if x.get(ns+'name')=='PORTAL_KEY'),None)
    if not meta:raise ValueError('Missing PORTAL_KEY')
    resources=apk.get_android_resources();encrypted_hosts=[]
    for name in ['portal_main','portal_backup']:
        value=resources.get_string(apk.get_package(),name)
        if not value:raise ValueError('Missing '+name)
        encrypted_hosts.append(value[1])
    # Do not assume an obfuscated method has the same purpose across APKs.
    # The old Lk2/b.g now returns a UUID in Focuz; Lib/c.c supplies the
    # 3DES candidate for its portal resources. Neither value is printed.
    hosts=None;portal_key=None
    for candidate in dict.fromkeys(root_candidates+([root] if root else [])):
        try:
            trial_hosts=[decrypt(value,candidate) for value in encrypted_hosts]
            trial_key=decrypt(meta,candidate)
        except (ValueError,UnicodeError):
            continue
        if all(re.fullmatch(r'[A-Za-z0-9.-]+(?::[0-9]+)?',h) for h in trial_hosts) and trial_key:
            hosts=trial_hosts
            portal_key=trial_key
            break
    if not hosts or portal_key is None:
        raise ValueError('No compatible portal decryption found; service.json was not modified')
    device=bytearray(uuid.uuid4().bytes[:6]);device[0]=2
    device_id=':'.join(f'{v:02x}' for v in device)
    installation_serial=uuid.uuid4().hex
    if output.exists():
        try:
            old=json.loads(output.read_text());device_id=old.get('device_id',device_id)
            installation_serial=old.get('installation_serial',installation_serial)
        except (ValueError,OSError):pass
    config={'portal_hosts':hosts,'key_base64':portal_key,'app_id':app_id,
        'apk_version':str(api_version),'manifest_version':apk.get_androidversion_code(),
        'installation_serial':installation_serial,'account_type':'auto','login_type':'2',
        'password_salt':salt,'password_aes_key':aes_key,
        'language':'es','device_id':device_id,'ca_file':'cacert.pem' if platform=='windows' else 'sdmc:/switch/xuperswitch/cacert.pem'}
    output.parent.mkdir(parents=True,exist_ok=True)
    output.write_text(json.dumps(config,indent=2)+'\n',encoding='utf-8')
    output.chmod(0o600)
    print('Configuracion generada. No contiene usuario ni contrasena.')

if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('apk',type=Path);parser.add_argument('--output',type=Path)
    parser.add_argument('--platform',choices=['switch','windows'],default='switch')
    parser.add_argument('--inspect-only',action='store_true',
                        help='Analisis local sin descifrar claves ni generar service.json')
    args=parser.parse_args()
    if args.inspect_only:
        from inspect_apk_compat import inspect
        print(json.dumps(inspect(args.apk),indent=2,ensure_ascii=False))
    else:
        if args.output is None:
            parser.error('--output es obligatorio al generar una configuracion')
        configure(args.apk,args.output,args.platform)
