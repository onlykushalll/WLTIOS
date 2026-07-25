#!/bin/bash
# sign-boot.sh — Sign the seL4 kernel image for verified boot
#
# This uses a SEPARATE key from the package signing key (different trust
# boundary). The boot key protects against evil-maid; the package key
# protects against malicious updates. Compromising one doesn't compromise
# the other.
#
# Usage: ./scripts/sign-boot.sh <kernel-image> [key-dir]
# Output: <kernel-image>.sig (64-byte detached Ed25519 signature)

set -e
KERNEL="${1:?Usage: sign-boot.sh <kernel-image> [key-dir]}"
KEY_DIR="${2:-keys/boot}"

# Generate the boot keypair if it doesn't exist
if [ ! -f "$KEY_DIR/boot-pubkey.bin" ]; then
    echo "=== Generating boot Ed25519 keypair ==="
    mkdir -p "$KEY_DIR"
    python3 -c "
from cryptography.hazmat.primitives.asymmetric.ed25519 import Ed25519PrivateKey
from cryptography.hazmat.primitives import serialization
import os
key = Ed25519PrivateKey.generate()
pub = key.public_key()
priv_bytes = key.private_bytes(serialization.Encoding.Raw, serialization.PrivateFormat.Raw, serialization.NoEncryption())
pub_bytes = pub.public_bytes(serialization.Encoding.Raw, serialization.PublicFormat.Raw)
open('$KEY_DIR/boot-pubkey.bin','wb').write(pub_bytes)
open('$KEY_DIR/boot-privkey.bin','wb').write(priv_bytes)
print(f'Boot keypair: pubkey={pub_bytes.hex()[:16]}... privkey={priv_bytes.hex()[:16]}...')
"
fi

# Sign the kernel image
echo "=== Signing $KERNEL ==="
python3 -c "
from cryptography.hazmat.primitives.asymmetric.ed25519 import Ed25519PrivateKey
from cryptography.hazmat.primitives import serialization
import sys
priv = open('$KEY_DIR/boot-privkey.bin','rb').read()
key = Ed25519PrivateKey.from_private_bytes(priv)
data = open('$KERNEL','rb').read()
sig = key.sign(data)
open('$KERNEL.sig','wb').write(sig)
print(f'Signature: {len(sig)} bytes, sha256(data)={__import__(\"hashlib\").sha256(data).hexdigest()[:16]}...')
"

echo "=== Done: $KERNEL.sig created ==="
ls -la "$KERNEL.sig"
