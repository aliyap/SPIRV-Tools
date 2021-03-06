// Copyright (c) 2016 Google Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef LIBSPIRV_TEST_OPT_PASS_FIXTURE_H_
#define LIBSPIRV_TEST_OPT_PASS_FIXTURE_H_

#include <iostream>
#include <string>
#include <tuple>
#include <vector>

#include <gtest/gtest.h>

#include "opt/build_module.h"
#include "opt/make_unique.h"
#include "opt/pass_manager.h"
#include "opt/passes.h"
#include "spirv-tools/libspirv.hpp"

namespace spvtools {

// Template class for testing passes. It contains some handy utility methods for
// running passes and checking results.
//
// To write value-Parameterized tests:
//   using ValueParamTest = PassTest<::testing::TestWithParam<std::string>>;
// To use as normal fixture:
//   using FixtureTest = PassTest<::testing::Test>;
template <typename TestT>
class PassTest : public TestT {
 public:
  PassTest()
      : consumer_(nullptr),
        tools_(SPV_ENV_UNIVERSAL_1_1),
        manager_(new opt::PassManager()) {}

  // Runs the given |pass| on the binary assembled from the |assembly|, and
  // disassebles the optimized binary. Returns a tuple of disassembly string
  // and the boolean value returned from pass Process() function.
  std::tuple<std::string, opt::Pass::Status> OptimizeAndDisassemble(
      opt::Pass* pass, const std::string& original, bool skip_nop) {
    std::unique_ptr<ir::Module> module =
        BuildModule(SPV_ENV_UNIVERSAL_1_1, consumer_, original);
    EXPECT_NE(nullptr, module) << "Assembling failed for shader:\n"
                               << original << std::endl;
    if (!module) {
      return std::make_tuple(std::string(), opt::Pass::Status::Failure);
    }

    const auto status = pass->Process(module.get());

    std::vector<uint32_t> binary;
    module->ToBinary(&binary, skip_nop);
    std::string optimized;
    EXPECT_TRUE(tools_.Disassemble(binary, &optimized))
        << "Disassembling failed for shader:\n"
        << original << std::endl;
    return std::make_tuple(optimized, status);
  }

  // Runs a single pass of class |PassT| on the binary assembled from the
  // |assembly|, disassembles the optimized binary. Returns a tuple of
  // disassembly string and the boolean value from the pass Process() function.
  template <typename PassT, typename... Args>
  std::tuple<std::string, opt::Pass::Status> SinglePassRunAndDisassemble(
      const std::string& assembly, bool skip_nop, Args&&... args) {
    auto pass = MakeUnique<PassT>(std::forward<Args>(args)...);
    pass->SetMessageConsumer(consumer_);
    return OptimizeAndDisassemble(pass.get(), assembly, skip_nop);
  }

  // Runs a single pass of class |PassT| on the binary assembled from the
  // |original| assembly, and checks whether the optimized binary can be
  // disassembled to the |expected| assembly. This does *not* involve pass
  // manager. Callers are suggested to use SCOPED_TRACE() for better messages.
  template <typename PassT, typename... Args>
  void SinglePassRunAndCheck(const std::string& original,
                             const std::string& expected, bool skip_nop,
                             Args&&... args) {
    std::string optimized;
    auto status = opt::Pass::Status::SuccessWithoutChange;
    std::tie(optimized, status) = SinglePassRunAndDisassemble<PassT>(
        original, skip_nop, std::forward<Args>(args)...);
    // Check whether the pass returns the correct modification indication.
    EXPECT_NE(opt::Pass::Status::Failure, status);
    EXPECT_EQ(original == expected,
              status == opt::Pass::Status::SuccessWithoutChange);
    EXPECT_EQ(expected, optimized);
  }

  // Adds a pass to be run.
  template <typename PassT, typename... Args>
  void AddPass(Args&&... args) {
    manager_->AddPass<PassT>(std::forward<Args>(args)...);
  }

  // Renews the pass manager, including clearing all previously added passes.
  void RenewPassManger() {
    manager_.reset(new opt::PassManager());
    manager_->SetMessageConsumer(consumer_);
  }

  // Runs the passes added thus far using a pass manager on the binary assembled
  // from the |original| assembly, and checks whether the optimized binary can
  // be disassembled to the |expected| assembly. Callers are suggested to use
  // SCOPED_TRACE() for better messages.
  void RunAndCheck(const std::string& original, const std::string& expected) {
    assert(manager_->NumPasses());

    std::unique_ptr<ir::Module> module =
        BuildModule(SPV_ENV_UNIVERSAL_1_1, nullptr, original);
    ASSERT_NE(nullptr, module);

    manager_->Run(module.get());

    std::vector<uint32_t> binary;
    module->ToBinary(&binary, /* skip_nop = */ false);

    std::string optimized;
    EXPECT_TRUE(tools_.Disassemble(binary, &optimized));
    EXPECT_EQ(expected, optimized);
  }

 private:
  MessageConsumer consumer_;  // Message consumer.
  SpirvTools tools_;  // An instance for calling SPIRV-Tools functionalities.
  std::unique_ptr<opt::PassManager> manager_;  // The pass manager.
};

}  // namespace spvtools

#endif  // LIBSPIRV_TEST_OPT_PASS_FIXTURE_H_
