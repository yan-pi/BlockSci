import blocksci
import pytest


def expected_stack(entry):
    return [bytes.fromhex(item) for item in entry["witness_stack"]]


def funding_output(chain, manifest, entry):
    funding = chain.tx_with_hash(manifest["funding_txid"])
    return funding.outputs[entry["output"]]


def assert_taproot_output(chain, manifest, entry):
    output = funding_output(chain, manifest, entry)
    address = chain.address_from_string(entry["address"])

    assert output.address_type == blocksci.address_type.witness_taproot
    assert isinstance(address, blocksci.TaprootAddress)
    assert output.address == address
    assert address.address_string == entry["address"]
    assert address.output_key == entry["output_key"]
    return output, address


@pytest.mark.btc
def test_taproot_fixture_matches_expected_tip(taproot_chain, chain_name):
    chain, manifest = taproot_chain

    assert len(chain) == manifest["tip_height"] + 1
    assert str(chain[-1].hash) == manifest["tip_hash"]


@pytest.mark.btc
def test_unspent_taproot_output_has_no_witness(taproot_chain, chain_name):
    chain, manifest = taproot_chain
    output, address = assert_taproot_output(chain, manifest, manifest["unspent"])

    assert not output.is_spent
    assert address.witness_stack is None


@pytest.mark.btc
def test_taproot_key_path_spend_round_trips_witness(taproot_chain, chain_name):
    chain, manifest = taproot_chain
    entry = manifest["key_path"]
    output, address = assert_taproot_output(chain, manifest, entry)
    spending_tx = chain.tx_with_hash(entry["spending_txid"])

    assert output.is_spent
    assert spending_tx.inputs[0].address_type == blocksci.address_type.witness_taproot
    assert address.witness_stack == expected_stack(entry)
    assert len(address.witness_stack) == 1
    assert len(address.witness_stack[0]) in (64, 65)


@pytest.mark.btc
def test_taproot_script_path_spend_preserves_stack_items(taproot_chain, chain_name):
    chain, manifest = taproot_chain
    entry = manifest["script_path"]
    output, address = assert_taproot_output(chain, manifest, entry)
    spending_tx = chain.tx_with_hash(entry["spending_txid"])

    assert output.is_spent
    assert spending_tx.inputs[0].address_type == blocksci.address_type.witness_taproot
    assert address.witness_stack == expected_stack(entry)
    assert len(address.witness_stack) == 3
