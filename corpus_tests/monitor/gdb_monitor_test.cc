// Copyright 2020 Google LLC
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

#include "corpus_tests/monitor/gdb_monitor.h"

#include <iostream>

#include "gtest/gtest.h"

namespace corpus_tests {
namespace {

TEST(GdbMonitor, TestPrintStopReply) {
  GdbMonitor monitor(0);
  std::string expected_output = "The program received signal: 05\n";
  testing::internal::CaptureStdout();
  monitor.PrintStopReply("S05");
  std::string output = testing::internal::GetCapturedStdout();
  EXPECT_EQ(output, expected_output);

  expected_output = "There are no resumed threads left in the target.\n";
  testing::internal::CaptureStdout();
  monitor.PrintStopReply("N");
  output = testing::internal::GetCapturedStdout();
  EXPECT_EQ(output, expected_output);

  expected_output = "The program received signal: 01, thread:0000\n";
  testing::internal::CaptureStdout();
  monitor.PrintStopReply("T01thread:0000");
  output = testing::internal::GetCapturedStdout();
  EXPECT_EQ(output, expected_output);

  expected_output = "The process exited with exit status: 02, process:22831\n";
  testing::internal::CaptureStdout();
  monitor.PrintStopReply("W02;process:22831");
  output = testing::internal::GetCapturedStdout();
  EXPECT_EQ(output, expected_output);

  expected_output = "The process terminated with signal: 10, process:0\n";
  testing::internal::CaptureStdout();
  monitor.PrintStopReply("X10;process:0");
  output = testing::internal::GetCapturedStdout();
  EXPECT_EQ(output, expected_output);

  expected_output = "";
  testing::internal::CaptureStdout();
  monitor.PrintStopReply("INVALID_RESPONSE");
  output = testing::internal::GetCapturedStdout();
  EXPECT_EQ(output, expected_output);
}

}  // namespace
}  // namespace corpus_tests
