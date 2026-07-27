#!/usr/bin/env python3
"""Generate the committed Taproot regtest fixture used by BlockSci tests."""

from __future__ import annotations

import argparse
import base64
import hashlib
import json
import resource
import shutil
import struct
import subprocess
import tempfile
import time
import urllib.error
import urllib.parse
import urllib.request
from pathlib import Path
from typing import Final

RPC_USER: Final = "blocksci"
RPC_PASSWORD: Final = "blocksci-taproot-fixture"
RPC_PORT: Final = 18443
GENERATOR_X_ONLY_KEY: Final = "79be667ef9dcbbac55a06295ce870b07029bfcdb2dce28d959f2815b16f81798"


def base58check(payload: bytes) -> str:
    """Encode bytes using Bitcoin's Base58Check representation."""
    alphabet = "123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz"
    checksum = hashlib.sha256(hashlib.sha256(payload).digest()).digest()[:4]
    encoded = payload + checksum
    value = int.from_bytes(encoded, "big")
    result = ""
    while value:
        value, remainder = divmod(value, 58)
        result = alphabet[remainder] + result
    leading_zeroes = len(encoded) - len(encoded.lstrip(b"\0"))
    return "1" * leading_zeroes + result


def regtest_wif(secret: int) -> str:
    """Return a compressed regtest WIF for a deterministic test-only key."""
    return base58check(b"\xef" + secret.to_bytes(32, "big") + b"\x01")


class RpcClient:
    """Minimal Bitcoin Core JSON-RPC client."""

    def __init__(self, username: str, password: str, port: int = RPC_PORT) -> None:
        credentials = base64.b64encode(f"{username}:{password}".encode()).decode()
        self._authorization = f"Basic {credentials}"
        self._port = port
        self._request_id = 0

    def call(self, method: str, params: list[object] | dict[str, object] | None = None, wallet: str | None = None):
        self._request_id += 1
        wallet_path = "" if wallet is None else f"/wallet/{urllib.parse.quote(wallet, safe='')}"
        request = urllib.request.Request(
            f"http://127.0.0.1:{self._port}{wallet_path}",
            data=json.dumps(
                {
                    "jsonrpc": "2.0",
                    "id": self._request_id,
                    "method": method,
                    "params": [] if params is None else params,
                }
            ).encode(),
            headers={"Authorization": self._authorization, "Content-Type": "application/json"},
        )
        try:
            with urllib.request.urlopen(request, timeout=30) as response:
                result = json.load(response)
        except urllib.error.HTTPError as error:
            detail = error.read().decode()
            raise RuntimeError(f"RPC {method} failed: {detail}") from error
        if result.get("error") is not None:
            raise RuntimeError(f"RPC {method} failed: {result['error']}")
        return result["result"]


def wait_for_rpc(rpc: RpcClient) -> None:
    """Wait until the regtest node accepts RPC requests."""
    deadline = time.monotonic() + 60
    while time.monotonic() < deadline:
        try:
            rpc.call("getblockchaininfo")
            return
        except (OSError, RuntimeError):
            time.sleep(0.25)
    raise RuntimeError("Bitcoin Core RPC did not become ready")


def descriptor(rpc: RpcClient, expression: str) -> tuple[str, str]:
    """Return checksummed private and normalized public descriptors."""
    info = rpc.call("getdescriptorinfo", [expression])
    return f"{expression}#{info['checksum']}", info["descriptor"]


def import_descriptor(rpc: RpcClient, wallet: str, private_descriptor: str) -> None:
    result = rpc.call(
        "importdescriptors",
        [[{"desc": private_descriptor, "timestamp": 0, "active": False}]],
        wallet=wallet,
    )
    if not result[0]["success"]:
        raise RuntimeError(f"Descriptor import failed: {result[0]}")


def spend_exact_output(rpc: RpcClient, wallet: str, utxo: dict[str, object], destination: str) -> str:
    """Create and broadcast a wallet-signed transaction spending one selected output."""
    amount = utxo["amount"]
    if not isinstance(amount, (int, float)):
        raise TypeError("Unexpected listunspent amount")
    params: list[object] = [
        [{"txid": utxo["txid"], "vout": utxo["vout"]}],
        [{destination: float(amount)}],
        0,
        {"add_inputs": False, "subtractFeeFromOutputs": [0]},
        True,
    ]
    funded = rpc.call(
        "walletcreatefundedpsbt",
        params,
        wallet=wallet,
    )
    processed = rpc.call("walletprocesspsbt", [funded["psbt"]], wallet=wallet)
    if not processed["complete"]:
        raise RuntimeError("Wallet could not sign the selected Taproot output")
    finalized = rpc.call("finalizepsbt", [processed["psbt"]])
    if not finalized["complete"]:
        raise RuntimeError("Taproot PSBT did not finalize")
    return rpc.call("sendrawtransaction", [finalized["hex"]])


def fund_outputs(rpc: RpcClient, wallet: str, outputs: dict[str, float], change_address: str) -> str:
    """Fund and broadcast outputs while using an explicit deterministic change address."""
    output_list = [{address: amount} for address, amount in outputs.items()]
    params: list[object] = [[], output_list, 0, {"changeAddress": change_address}, True]
    funded = rpc.call("walletcreatefundedpsbt", params, wallet=wallet)
    processed = rpc.call("walletprocesspsbt", [funded["psbt"]], wallet=wallet)
    if not processed["complete"]:
        raise RuntimeError("Miner wallet could not sign the funding transaction")
    finalized = rpc.call("finalizepsbt", [processed["psbt"]])
    if not finalized["complete"]:
        raise RuntimeError("Funding PSBT did not finalize")
    return rpc.call("sendrawtransaction", [finalized["hex"]])


def create_taproot_chain(rpc: RpcClient) -> dict[str, object]:
    """Create confirmed key-path and script-path Taproot spends."""
    miner_wallet = "taproot-miner"
    taproot_wallet = "taproot-spender"
    rpc.call(
        "createwallet",
        {"wallet_name": miner_wallet, "blank": True, "descriptors": True},
    )
    rpc.call("createwallet", {"wallet_name": taproot_wallet, "blank": True, "descriptors": True})

    mining_private, mining_public = descriptor(rpc, f"wpkh({regtest_wif(5)})")
    destination_private, destination_public = descriptor(rpc, f"wpkh({regtest_wif(6)})")
    key_private, key_public = descriptor(rpc, f"tr({regtest_wif(3)})")
    script_private, script_public = descriptor(
        rpc,
        f"tr({GENERATOR_X_ONLY_KEY},pk({regtest_wif(2)}))",
    )
    unspent_private, unspent_public = descriptor(rpc, f"tr({regtest_wif(4)})")
    for private in (key_private, script_private, unspent_private):
        import_descriptor(rpc, taproot_wallet, private)
    for private in (mining_private, destination_private):
        import_descriptor(rpc, miner_wallet, private)

    key_address = rpc.call("deriveaddresses", [key_public])[0]
    script_address = rpc.call("deriveaddresses", [script_public])[0]
    unspent_address = rpc.call("deriveaddresses", [unspent_public])[0]

    mining_address = rpc.call("deriveaddresses", [mining_public])[0]
    destination = rpc.call("deriveaddresses", [destination_public])[0]
    rpc.call("setmocktime", [1_700_000_000])
    rpc.call("generatetoaddress", [101, mining_address])
    funding_txid = fund_outputs(
        rpc,
        miner_wallet,
        {key_address: 1.0, script_address: 1.0, unspent_address: 1.0},
        mining_address,
    )
    rpc.call("generatetoaddress", [1, mining_address])

    utxos = {
        item["address"]: item
        for item in rpc.call(
            "listunspent", [1, 9999999, [key_address, script_address, unspent_address]], wallet=taproot_wallet
        )
    }
    key_spend_txid = spend_exact_output(rpc, taproot_wallet, utxos[key_address], destination)
    script_spend_txid = spend_exact_output(rpc, taproot_wallet, utxos[script_address], destination)
    rpc.call("generatetoaddress", [1, mining_address])

    funding = rpc.call("getrawtransaction", [funding_txid, True])
    key_spend = rpc.call("getrawtransaction", [key_spend_txid, True])
    script_spend = rpc.call("getrawtransaction", [script_spend_txid, True])
    tip_height = rpc.call("getblockcount")
    tip_hash = rpc.call("getblockhash", [tip_height])

    def output_for(address: str) -> dict[str, object]:
        return next(output for output in funding["vout"] if output["scriptPubKey"].get("address") == address)

    def taproot_output(address: str) -> dict[str, object]:
        output = output_for(address)
        script_pubkey = output["scriptPubKey"]
        if not isinstance(script_pubkey, dict):
            raise RuntimeError(f"Missing scriptPubKey for {address}")
        script_hex = script_pubkey["hex"]
        if not isinstance(script_hex, str) or not script_hex.startswith("5120"):
            raise RuntimeError(f"Expected a P2TR script for {address}")
        return {"address": address, "output": output["n"], "output_key": script_hex[4:]}

    return {
        "tip_height": tip_height,
        "tip_hash": tip_hash,
        "funding_txid": funding_txid,
        "unspent": taproot_output(unspent_address),
        "key_path": {
            **taproot_output(key_address),
            "spending_txid": key_spend_txid,
            "witness_stack": key_spend["vin"][0]["txinwitness"],
        },
        "script_path": {
            **taproot_output(script_address),
            "spending_txid": script_spend_txid,
            "witness_stack": script_spend["vin"][0]["txinwitness"],
        },
    }


def start_direct_node(work_dir: Path, blocks_xor: bool) -> tuple[subprocess.Popen[str], Path, RpcClient]:
    soft_limit, hard_limit = resource.getrlimit(resource.RLIMIT_NOFILE)
    if soft_limit == resource.RLIM_INFINITY:
        resource.setrlimit(resource.RLIMIT_NOFILE, (1024, hard_limit))
    datadir = work_dir / "bitcoin"
    datadir.mkdir(parents=True)
    command = [
        "bitcoind",
        "-regtest",
        f"-datadir={datadir}",
        f"-blocksxor={int(blocks_xor)}",
        "-txindex=1",
        "-fallbackfee=0.00001",
        f"-rpcuser={RPC_USER}",
        f"-rpcpassword={RPC_PASSWORD}",
        f"-rpcport={RPC_PORT}",
        "-printtoconsole",
    ]
    process = subprocess.Popen(command, stdout=subprocess.DEVNULL, stderr=subprocess.STDOUT, text=True)
    rpc = RpcClient(RPC_USER, RPC_PASSWORD)
    wait_for_rpc(rpc)
    return process, datadir / "regtest", rpc


def provision_nigiri(work_dir: Path, blocks_xor: bool) -> tuple[Path, RpcClient]:
    subprocess.run(["nigiri", "--datadir", str(work_dir), "version"], check=True)
    config = work_dir / "volumes" / "bitcoin" / "bitcoin.conf"
    with config.open("a", encoding="utf-8") as stream:
        stream.write(f"\nblocksxor={int(blocks_xor)}\n")
    subprocess.run(
        [
            "docker",
            "compose",
            "--file",
            str(work_dir / "docker-compose.yml"),
            "--project-name",
            "nigiri",
            "up",
            "--detach",
            "bitcoin",
        ],
        check=True,
    )
    rpc = RpcClient("admin1", "123")
    wait_for_rpc(rpc)
    return work_dir / "volumes" / "bitcoin" / "regtest", rpc


def stop_nigiri(work_dir: Path) -> None:
    compose = work_dir / "docker-compose.yml"
    if compose.exists():
        subprocess.run(
            ["docker", "compose", "--file", str(compose), "--project-name", "nigiri", "down"],
            check=False,
        )


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        while chunk := stream.read(1024 * 1024):
            digest.update(chunk)
    return digest.hexdigest()


def trim_preallocated_block_file(path: Path, xor_key: bytes) -> None:
    """Remove Bitcoin Core's unused zero-filled block-file allocation."""
    regtest_magic = bytes.fromhex("fabfb5da")
    end = 0
    with path.open("rb") as stream:
        while True:
            header = stream.read(8)
            if not header or header == b"\0" * len(header):
                break
            decoded_header = bytes(byte ^ xor_key[(end + index) % len(xor_key)] for index, byte in enumerate(header))
            if len(decoded_header) != 8 or decoded_header[:4] != regtest_magic:
                raise RuntimeError(f"Unexpected block framing at offset {end} in {path}")
            block_size = struct.unpack("<I", decoded_header[4:])[0]
            stream.seek(block_size, 1)
            end += 8 + block_size
    with path.open("r+b") as stream:
        stream.truncate(end)


def export_fixture(
    chain_dir: Path,
    output_dir: Path,
    manifest: dict[str, object],
    backend: str,
    version: str,
    blocks_xor: bool,
) -> None:
    blocks_output = output_dir / "regtest" / "blocks"
    if output_dir.exists():
        shutil.rmtree(output_dir)
    blocks_output.mkdir(parents=True)
    block_files = sorted((chain_dir / "blocks").glob("blk*.dat"))
    if not block_files:
        raise RuntimeError("Bitcoin Core did not create any block files")
    xor_source = chain_dir / "blocks" / "xor.dat"
    xor_key = xor_source.read_bytes() if xor_source.exists() else b"\0" * 8
    if len(xor_key) != 8:
        raise RuntimeError(f"Unexpected Bitcoin Core XOR key length: {len(xor_key)}")
    checksums: dict[str, str] = {}
    for source in block_files:
        destination = blocks_output / source.name
        shutil.copy2(source, destination)
        trim_preallocated_block_file(destination, xor_key)
        checksums[str(destination.relative_to(output_dir))] = sha256_file(destination)
    if blocks_xor:
        xor_destination = blocks_output / "xor.dat"
        shutil.copy2(xor_source, xor_destination)
        checksums[str(xor_destination.relative_to(output_dir))] = sha256_file(xor_destination)
    manifest.update(
        {
            "generator_backend": backend,
            "bitcoin_core_version": version,
            "blocksxor": blocks_xor,
            "files": checksums,
        }
    )
    (output_dir / "fixture-manifest.json").write_text(json.dumps(manifest, indent=2, sort_keys=True) + "\n")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--backend", choices=("direct", "nigiri"), default="nigiri")
    parser.add_argument("--blocksxor", choices=("disabled", "enabled"), default="disabled")
    parser.add_argument("--output", type=Path, default=Path("test/files/btc-taproot"))
    parser.add_argument("--work-dir", type=Path)
    args = parser.parse_args()

    temporary = None
    if args.work_dir is None:
        temporary = tempfile.TemporaryDirectory(prefix="blocksci-taproot-")
        work_dir = Path(temporary.name)
    else:
        work_dir = args.work_dir.resolve()
        if work_dir.exists():
            shutil.rmtree(work_dir)
        work_dir.mkdir(parents=True)

    process: subprocess.Popen[str] | None = None
    rpc: RpcClient | None = None
    blocks_xor = args.blocksxor == "enabled"
    try:
        if args.backend == "nigiri":
            chain_dir, rpc = provision_nigiri(work_dir, blocks_xor)
        else:
            process, chain_dir, rpc = start_direct_node(work_dir, blocks_xor)
        version = rpc.call("getnetworkinfo")["subversion"]
        manifest = create_taproot_chain(rpc)
        rpc.call("stop")
        if process is not None:
            process.wait(timeout=30)
        else:
            stop_nigiri(work_dir)
        export_fixture(chain_dir, args.output.resolve(), manifest, args.backend, version, blocks_xor)
    finally:
        if process is not None and process.poll() is None:
            process.terminate()
            process.wait(timeout=30)
        if args.backend == "nigiri":
            stop_nigiri(work_dir)
        if temporary is not None:
            temporary.cleanup()


if __name__ == "__main__":
    main()
