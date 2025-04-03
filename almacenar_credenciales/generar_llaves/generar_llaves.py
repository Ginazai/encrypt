import os
import json
from hashlib import sha256

def generate_aes_key(size: int):
    """Generates a random AES key of the specified size (in bytes)."""
    if size not in [16, 32]:
        raise ValueError("AES key size must be 16 or 32 bytes.")
    output = []
    for x in range(size):
        output.append(os.urandom(1).hex())
    hex_out = []
    for item in output:
        hex_out.append(f"0x{item}")
    return json.dumps(hex_out)
    #return os.urandom(size)
def generate_jwt_hash(rounds: int):
    return sha256(os.urandom(rounds)).hexdigest()

# Generate keys
key_16 = generate_aes_key(16)
key_32 = generate_aes_key(32)
jwt_hash = generate_jwt_hash(9000)

# Print keys in hexadecimal format
print(f"AES 16-byte key: {key_16}")
print(f"AES 32-byte key: {key_32}")
print(f"JWT 32-byte key: {jwt_hash}")