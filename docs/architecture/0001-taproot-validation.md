# ADR 0001: Validate Taproot with committed regtest data

## Status

Accepted

## Context

Taproot adds a persisted address type, x-only output keys, Bech32m addresses,
and witness stacks whose item boundaries must be preserved. Compilation and
enum smoke tests cannot prove that the disk parser, indexes, C++ API, and
Python bindings work together. Running Docker in every required check would
make the existing Nix-first CI slower and less reproducible. Bitcoin Core 28+
also XORs newly created block files by default, while BlockSci issue #4 tracks
support for that storage format.

## Decision

BlockSci keeps compact plaintext and XOR-obfuscated Bitcoin regtest fixtures
containing an unspent P2TR output, a key-path spend, and a script-path spend.
The normal test suite parses both committed `blk*.dat` variants without Docker.
A generator supports Nigiri for live validation and direct Bitcoin Core for
development.

Witness stacks are persisted as a CompactSize item count followed by
CompactSize item lengths and bytes. Parsed data version 6 is required because
the address enums and script storage layout changed.

`SafeMemReader` decodes scalar values using their absolute block-file offsets.
The transaction pipeline materializes only the current decoded block payload,
and each in-flight transaction retains shared ownership of that immutable
buffer. This preserves script, witness, and transaction-hash pointer lifetimes
without copying complete block files.

An optional scheduled/manual Nigiri workflow regenerates and parses the same
scenario with both `blocksxor=0` and `blocksxor=1`, covering issue #4.

## Consequences

Pull-request checks remain deterministic and Docker-free. Key-path and
script-path witness bytes are covered end to end, including multiple stack
items. Existing parsed datasets must be rebuilt. The fixture must be
regenerated when its scenario or persisted format changes. Bitcoin Core block
directories with a valid eight-byte `blocks/xor.dat` key can be parsed directly.
