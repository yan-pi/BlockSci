import hashlib
import json
import os
import subprocess

import pytest

_taproot_fixture_override = os.environ.get("BLOCKSCI_TAPROOT_FIXTURE_DIR")
_taproot_fixture_params = (
    (_taproot_fixture_override,) if _taproot_fixture_override else ("../files/btc-taproot", "../files/btc-taproot-xor")
)


def pytest_addoption(parser):
    parser.addoption("--btc", action="store_true", help="Run tests for Bitcoin")
    parser.addoption("--bch", action="store_true", help="Run tests for Bitcoin Cash")
    parser.addoption("--ltc", action="store_true", help="Run tests for Litecoin")


def pytest_generate_tests(metafunc):
    metafunc.fixturenames.append("chain_name")
    chains = []
    if metafunc.config.option.btc:
        chains += ["btc"]
    if metafunc.config.option.bch:
        chains += ["bch"]
    if metafunc.config.option.ltc:
        chains += ["ltc"]
    if not chains:
        chains = ["btc", "bch", "ltc"]
    metafunc.parametrize("chain_name", chains, scope="session")


def pytest_runtest_call(item):
    markers = [x.name for x in item.iter_markers()]
    if markers and item.funcargs["chain_name"] not in markers:
        pytest.skip("Skipping test for chain {}".format(item.funcargs["chain_name"]))


@pytest.fixture(scope="session")
def chain(tmpdir_factory, chain_name):
    temp_dir = tmpdir_factory.mktemp(chain_name)
    chain_dir = str(temp_dir)
    self_dir = os.path.dirname(os.path.realpath(__file__))

    if chain_name == "btc":
        blocksci_chain_name = "bitcoin_regtest"
    elif chain_name == "bch":
        blocksci_chain_name = "bitcoin_cash_regtest"
    elif chain_name == "ltc":
        blocksci_chain_name = "litecoin_regtest"
    else:
        raise ValueError(f"Invalid chain name {chain_name}")

    create_config_cmd = [
        "blocksci_parser",
        chain_dir + "/config.json",
        "generate-config",
        blocksci_chain_name,
        chain_dir,
        "--disk",
        f"{self_dir}/../files/{chain_name}/regtest/",
        "--max-block",
        "100",
    ]
    parse_cmd = ["blocksci_parser", chain_dir + "/config.json", "update"]

    # Parse the chain up to block 100 only
    subprocess.run(create_config_cmd, check=True)
    subprocess.run(parse_cmd, check=True)

    # Now parse the remainder of the chain
    subprocess.run(create_config_cmd[:-2], check=True)
    subprocess.run(parse_cmd, check=True)

    import blocksci

    chain = blocksci.Blockchain(chain_dir + "/config.json")
    return chain


@pytest.fixture
def json_data(chain_name):
    with open(f"../files/{chain_name}/output.json") as f:
        return json.load(f)


@pytest.fixture(scope="session", params=_taproot_fixture_params, ids=lambda path: os.path.basename(path))
def taproot_chain(tmpdir_factory, request):
    """Parse the dedicated P2TR fixture incrementally and return its manifest."""
    self_dir = os.path.dirname(os.path.realpath(__file__))
    fixture_dir = request.param if os.path.isabs(request.param) else os.path.join(self_dir, request.param)
    temp_dir = tmpdir_factory.mktemp(os.path.basename(fixture_dir))
    chain_dir = str(temp_dir)
    config_path = chain_dir + "/config.json"
    with open(fixture_dir + "/fixture-manifest.json") as stream:
        manifest = json.load(stream)
    for relative_path, expected_hash in manifest["files"].items():
        digest = hashlib.sha256()
        with open(fixture_dir + "/" + relative_path, "rb") as fixture_file:
            for chunk in iter(lambda: fixture_file.read(1024 * 1024), b""):
                digest.update(chunk)
        assert digest.hexdigest() == expected_hash
    create_config_cmd = [
        "blocksci_parser",
        config_path,
        "generate-config",
        "bitcoin_regtest",
        chain_dir,
        "--disk",
        fixture_dir + "/regtest",
        "--max-block",
        "102",
    ]
    subprocess.run(create_config_cmd, check=True)
    subprocess.run(["blocksci_parser", config_path, "update"], check=True)
    subprocess.run(create_config_cmd[:-2], check=True)
    subprocess.run(["blocksci_parser", config_path, "update"], check=True)
    subprocess.run(["blocksci_check_integrity", config_path], check=True)

    with open(config_path) as stream:
        assert json.load(stream)["version"] == 6
    import blocksci

    return blocksci.Blockchain(config_path), manifest
