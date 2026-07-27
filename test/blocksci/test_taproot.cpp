#include <blocksci/core/address_types.hpp>
#include <blocksci/core/witness_program.hpp>

#include <gtest/gtest.h>

namespace blocksci {

  TEST(TaprootAddressType, ExposesWitnessTaprootType) {
    EXPECT_EQ(AddressType::size, 11U);
    EXPECT_EQ(AddressType::WITNESS_TAPROOT, static_cast<AddressType::Enum>(9));
    EXPECT_EQ(AddressType::WITNESS_UNKNOWN, static_cast<AddressType::Enum>(10));
  }

  TEST(WitnessProgramClassification, ClassifiesVersionOneWith32ByteProgramAsTaproot) {
    EXPECT_EQ(classifyWitnessProgram(1, 32), AddressType::WITNESS_TAPROOT);
  }

  TEST(WitnessProgramClassification, KeepsOtherVersionOneProgramsUnknown) {
    EXPECT_EQ(classifyWitnessProgram(1, 31), AddressType::WITNESS_UNKNOWN);
    EXPECT_EQ(classifyWitnessProgram(1, 33), AddressType::WITNESS_UNKNOWN);
  }

  TEST(WitnessProgramClassification, PreservesVersionZeroTypes) {
    EXPECT_EQ(classifyWitnessProgram(0, 20), AddressType::WITNESS_PUBKEYHASH);
    EXPECT_EQ(classifyWitnessProgram(0, 32), AddressType::WITNESS_SCRIPTHASH);
    EXPECT_EQ(classifyWitnessProgram(0, 31), AddressType::NONSTANDARD);
  }

} // namespace blocksci
