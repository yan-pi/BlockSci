# Taproot regtest fixture

This generator creates the dedicated Bitcoin regtest chain used by BlockSci's
Taproot integration tests. It creates an unspent P2TR output, a key-path spend,
and a script-path spend. The resulting `blk*.dat` files are committed so normal
tests remain deterministic and do not require Docker.

## Generate with Nigiri

Nigiri and Docker must be installed. The generator provisions a clean Nigiri
directory, sets `blocksxor=0` before Bitcoin Core creates its blocks directory,
starts only Nigiri's Bitcoin Core service, and stops Core before copying block
files. Starting only Core avoids unnecessary Esplora/Electrs dependencies and
host-port conflicts in CI.

```bash
python test/fixtures/taproot/generate.py --backend nigiri
```

Generate the equivalent fixture using Bitcoin Core's default XOR-obfuscated
block storage:

```bash
python test/fixtures/taproot/generate.py \
  --backend nigiri \
  --blocksxor enabled \
  --output test/files/btc-taproot-xor
```

## Generate with a local Bitcoin Core

The direct backend is useful for development and produces the same scenario:

```bash
python test/fixtures/taproot/generate.py --backend direct
```

All embedded private keys are deterministic regtest-only keys and must never be
used on a public network. The default fixture disables block-file XOR, while
`--blocksxor enabled` preserves `xor.dat` and the obfuscated `blk*.dat` bytes to
exercise Bitcoin Core 28+ storage compatibility.
