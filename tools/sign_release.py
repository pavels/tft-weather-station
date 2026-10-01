#!/usr/bin/env python3
"""Creates and checks signed OTA release manifests. Needs the openssl CLI.

firmware.manifest is two lines:
    {"name":"tft_weather_station","version":"1.0.0","size":935696,"sha256":"<hex>"}
    <base64 RSA-2048 PKCS#1 v1.5 SHA-256 signature of exactly line 1, no newline>
The device verifies line 2 against line 1 with the public key compiled into the
firmware (components/ota_update/ota_public_key.pem), accepts only a newer
version, and checks the downloaded image's size and SHA-256 against line 1.

Usage:
    sign_release.py keygen <private_key.pem>
        New key pair; the public half goes to
        components/ota_update/ota_public_key.pem. Keep the private key out of
        the repo (CI secret OTA_SIGNING_KEY).
    sign_release.py sign <firmware.bin> <private_key.pem> <firmware.manifest>
        Version comes from version.h.
    sign_release.py verify <firmware.bin> <firmware.manifest> [public_key.pem]
"""
import base64
import hashlib
import json
import re
import subprocess
import sys
import tempfile
from pathlib import Path

PROJECT_DIR = Path(__file__).resolve().parent.parent
PUBLIC_KEY = PROJECT_DIR / "components" / "ota_update" / "ota_public_key.pem"
VERSION_H = PROJECT_DIR / "version.h"
PRODUCT_NAME = "tft_weather_station"


def openssl(*args, data=None):
    return subprocess.run(["openssl", *args], input=data, check=True, capture_output=True).stdout


def firmware_version():
    match = re.search(r'#define\s+OTAVERSION\s+"([^"]+)"', VERSION_H.read_text())
    if not match:
        sys.exit(f"{VERSION_H}: no OTAVERSION")
    return match.group(1)


def manifest_line(firmware):
    image = Path(firmware).read_bytes()
    manifest = {
        "name": PRODUCT_NAME,
        "version": firmware_version(),
        "size": len(image),
        "sha256": hashlib.sha256(image).hexdigest(),
    }
    return json.dumps(manifest, separators=(",", ":")).encode()


def keygen(private_key):
    if Path(private_key).exists():
        sys.exit(f"{private_key} exists, not overwriting")
    openssl("genrsa", "-out", private_key, "2048")
    PUBLIC_KEY.write_bytes(openssl("rsa", "-in", private_key, "-pubout"))
    print(f"private key: {private_key} (keep secret)\npublic key:  {PUBLIC_KEY}")


def sign(firmware, private_key, manifest_path):
    line = manifest_line(firmware)
    signature = openssl("dgst", "-sha256", "-sign", private_key, data=line)
    Path(manifest_path).write_bytes(line + b"\n" + base64.b64encode(signature) + b"\n")
    print(line.decode())


def verify(firmware, manifest_path, public_key=PUBLIC_KEY):
    line, signature_b64 = Path(manifest_path).read_bytes().split(b"\n")[:2]
    with tempfile.NamedTemporaryFile() as signature:
        signature.write(base64.b64decode(signature_b64))
        signature.flush()
        openssl("dgst", "-sha256", "-verify", str(public_key), "-signature", signature.name, data=line)

    manifest = json.loads(line)
    image = Path(firmware).read_bytes()
    if manifest["size"] != len(image) or manifest["sha256"] != hashlib.sha256(image).hexdigest():
        sys.exit("firmware doesn't match the manifest")
    print(f"OK: {manifest['name']} {manifest['version']}, {manifest['size']} bytes")


def main():
    commands = {"keygen": (keygen, 1), "sign": (sign, 3), "verify": (verify, 2)}
    if len(sys.argv) < 2 or sys.argv[1] not in commands:
        sys.exit(__doc__)
    function, required = commands[sys.argv[1]]
    args = sys.argv[2:]
    if len(args) < required:
        sys.exit(__doc__)
    try:
        function(*args)
    except subprocess.CalledProcessError as error:
        sys.exit(f"openssl failed: {error.stderr.decode().strip() or error}")


if __name__ == "__main__":
    main()
