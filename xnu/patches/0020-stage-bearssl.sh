#!/bin/bash
# Pinned, portable constant-time AES for the SWEET legacy CBC/ECB API.
#
# The source is BearSSL 7bea48e5e850ab4cafbe68d3765cdaba13a86d6f. www.bearssl.org answered every
# fetch with a 502 on 2026-11-04 (kernel run 41818516315 attempt 1 died there, and the same 502
# came back from a plain curl over a different network), so a build that depends on that one web
# server being up is not a build we control. The files this script stages -- and only those -- are
# therefore vendored under xnu/bearssl/ and used when the clone fails. The clone is still tried
# first, so upstream stays authoritative; the vendored tree holds the same pinned revision's
# files, and the one kernel-specific edit (machine/limits.h) is still made here rather than baked
# into it, so both paths produce the same bytes.
set -euo pipefail
[[ "${MACHINE_CONFIG:-}" =~ ^(SWEET|RAPHAEL)$ ]] || exit 0
[[ -f libkern/crypto/corecrypto_aes.c ]] || exit 1
bearssl_sha=7bea48e5e850ab4cafbe68d3765cdaba13a86d6f
here=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
vendor=${SWEET_BEARSSL_VENDOR:-$(cd "$here/../../xnu/bearssl" 2>/dev/null && pwd || true)}
bearssl_tmp=$(mktemp -d)
trap 'rm -rf "$bearssl_tmp"' EXIT
bearssl_src=""
if git init -q "$bearssl_tmp" &&
    git -C "$bearssl_tmp" fetch -q --depth 1 https://www.bearssl.org/git/BearSSL "$bearssl_sha"; then
	git -C "$bearssl_tmp" checkout -q FETCH_HEAD
	[[ "$(git -C "$bearssl_tmp" rev-parse HEAD)" == "$bearssl_sha" ]]
	bearssl_src=$bearssl_tmp
	echo "Fetched BearSSL $bearssl_sha from www.bearssl.org"
elif [[ -n "$vendor" && -f "$vendor/src/inner.h" ]]; then
	echo "www.bearssl.org could not be reached: staging BearSSL $bearssl_sha from $vendor"
	bearssl_src=$vendor
else
	echo "no BearSSL source: neither the clone nor a vendored tree at $vendor" >&2
	exit 1
fi
python3 - "$bearssl_src" <<'PY'
from pathlib import Path
import shutil
import sys

source = Path(sys.argv[1])
target = Path('libkern/crypto/sweet_bearssl')
if target.exists():
    raise SystemExit(f'refusing to overwrite existing AES source: {target}')
shutil.copytree(source / 'inc', target / 'inc')
(target / 'src/symcipher').mkdir(parents=True, exist_ok=True)
for file in ('inner.h', 'config.h'):
    shutil.copy2(source / 'src' / file, target / 'src' / file)
# XNU exports machine/limits.h instead of the userspace limits.h.
header = target / 'src/inner.h'
text = header.read_text()
assert text.count('#include <limits.h>') == 1
header.write_text(text.replace('#include <limits.h>',
    '#if defined(KERNEL)\n#include <machine/limits.h>\n'
    '#else\n#include <limits.h>\n#endif'))
for name in ('aes_ct', 'aes_ct_enc', 'aes_ct_dec', 'aes_ct_cbcenc', 'aes_ct_cbcdec'):
    shutil.copy2(source / 'src/symcipher' / (name + '.c'),
                 target / 'src/symcipher' / (name + '.c'))
# MD5, SHA-1 and SHA-384/512 for the legacy hash APIs, with their codecs.
for name in ('hash/md5.c', 'codec/dec32le.c', 'codec/enc32le.c',
             'hash/sha1.c', 'codec/dec32be.c', 'codec/enc32be.c',
             'hash/sha2big.c', 'codec/dec64be.c', 'codec/enc64be.c'):
    (target / 'src' / name).parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(source / 'src' / name, target / 'src' / name)
shutil.copy2(source / 'LICENSE.txt', target / 'LICENSE.txt')
PY
echo "Staged BearSSL portable AES, MD5, SHA-1 and SHA-384/512 ($bearssl_sha) for SWEET"
