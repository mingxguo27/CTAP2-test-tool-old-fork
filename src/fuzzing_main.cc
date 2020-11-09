#include <iostream>
#include <vector>

#include "absl/container/flat_hash_set.h"
#include "absl/strings/str_split.h"
#include "gflags/gflags.h"
#include "glog/logging.h"
#include "src/command_state.h"
#include "src/constants.h"
#include "src/fuzzing/fuzzer.h"
#include "src/hid/hid_device.h"
#include "src/monitors/blackbox_monitor.h"
#include "src/monitors/cortexm4_gdb_monitor.h"
#include "src/monitors/gdb_monitor.h"

static bool ValidatePort(const char* flagname, gflags::int32 value) {
  return value > 0 && value < 65535;
}

static bool ValidateMonitor(const char* flagname, const std::string& value) {
  const absl::flat_hash_set<std::string> kSupportedMonitors = {
      "blackbox", "cortexm4_gdb", "gdb"};
  return kSupportedMonitors.contains(value);
}

static bool ValidateFuzzingMode(const char* flagname, const std::string& value) {
  const absl::flat_hash_set<std::string> kSupportedModes = {
      "cbor_make_credential", "cbor_get_assertion", "cbor_client_pin", "ctap2_raw"};
  return kSupportedModes.contains(value);
}

DEFINE_string(
    token_path, "",
    "The path to the device on your operating system, usually /dev/hidraw*.");

DEFINE_string(
    corpus_path, "corpus_tests/test_corpus/",
    "The path to the corpus containing seed files to test the device.");

DEFINE_string(monitor, "blackbox", "The monitor type used in fuzzing.");
DEFINE_string(fuzzing_mode, "cbor_make_credential", "The type of inputs to be fuzzed.");

DEFINE_bool(verbose, false, "Printing debug logs, i.e. transmitted packets.");

DEFINE_int32(port, 2331, "Port to listen on for GDB remote connection.");
DEFINE_int32(num_runs, 0,
             "Number of inputs to be run. By default, the fuzzer will run "
             "indefinitely.");
DEFINE_int32(max_length, 0,
             "Maximum length of an input. By default, there is no limit.");
DEFINE_int32(max_mutation_degree, 10,
             "Maximum number of successive mutation operations to be applied. "
             "By default, value is 10.");
DEFINE_int32(seed, time(NULL), "The random seed for all random operations.");

DEFINE_validator(port, &ValidatePort);
DEFINE_validator(monitor, &ValidateMonitor);
DEFINE_validator(fuzzing_mode, &ValidateFuzzingMode);

int main(int argc, char** argv) {
  google::InitGoogleLogging(argv[0]);
  gflags::ParseCommandLineFlags(&argc, &argv, true);
  if (FLAGS_token_path.empty()) {
    std::cout << "Please add the --token_path flag for one of these devices:"
              << std::endl;
    fido2_tests::hid::PrintFidoDevices();
    return 0;
  }
  if (FLAGS_token_path == "_") {
    // This magic value is used by the run script for comfort.
    FLAGS_token_path = fido2_tests::hid::FindFirstFidoDevicePath();
    std::cout << "Testing device at path: " << FLAGS_token_path << std::endl;
  }

  fido2_tests::DeviceTracker tracker;
  std::unique_ptr<fido2_tests::DeviceInterface> device =
      std::make_unique<fido2_tests::hid::HidDevice>(&tracker, FLAGS_token_path,
                                                    FLAGS_verbose);
  CHECK(fido2_tests::Status::kErrNone == device->Init())
      << "CTAPHID initialization failed";
  device->Wink();
  std::cout << "This tool will irreversibly delete all credentials on your "
               "device. If one of your plugged security keys stores anything "
               "important, unplug it now before continuing."
            << std::endl;

  fido2_tests::CommandState command_state(device.get(), &tracker);
  std::unique_ptr<fido2_tests::Monitor> monitor;
  if (FLAGS_monitor == "blackbox") {
    monitor = std::make_unique<fido2_tests::BlackboxMonitor>();
  } else if (FLAGS_monitor == "cortexm4_gdb") {
    monitor = std::make_unique<fido2_tests::Cortexm4GdbMonitor>(FLAGS_port);
  } else if (FLAGS_monitor == "gdb") {
    monitor = std::make_unique<fido2_tests::GdbMonitor>(FLAGS_port);
  } else {
    CHECK(false) << "unreachable else - TEST SUITE BUG";
  }
  CHECK(monitor->Attach()) << "Monitor failed to attach!";
  CHECK(monitor->Prepare(&command_state)) << "Monitor preparation failed!";

  // Set up fuzzer.
  std::string corpus_dir = FLAGS_corpus_path;
  if (const char* env_dir = std::getenv("BUILD_WORKSPACE_DIRECTORY")) {
    corpus_dir = absl::StrCat(env_dir, "/", FLAGS_corpus_path);
  }
  fido2_tests::fuzzing_helpers::InputType fuzzing_input_type;
  if (FLAGS_fuzzing_mode == "cbor_make_credential") {
    fuzzing_input_type =
        fido2_tests::fuzzing_helpers::InputType::kCborMakeCredentialParameter;
  } else if (FLAGS_fuzzing_mode == "cbor_get_assertion") {
    fuzzing_input_type =
        fido2_tests::fuzzing_helpers::InputType::kCborGetAssertionParameter;
  } else if (FLAGS_fuzzing_mode == "cbor_client_pin") {
    fuzzing_input_type =
        fido2_tests::fuzzing_helpers::InputType::kCborClientPinParameter;
  } else if (FLAGS_fuzzing_mode == "ctap2_raw") {
    fuzzing_input_type = fido2_tests::fuzzing_helpers::InputType::kRawData;
  } else {
    CHECK(false) << "unreachable else - TEST SUITE BUG";
  }
  fido2_tests::Fuzzer fuzzer({corpus_dir, fuzzing_input_type, FLAGS_num_runs,
                              FLAGS_max_length,
                              FLAGS_max_mutation_degree, FLAGS_seed});
  fuzzer.Run(&command_state, device.get(), monitor.get());
  return 0;
}

