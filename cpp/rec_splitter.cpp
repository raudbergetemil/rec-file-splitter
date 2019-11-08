// Include the header-only libcluon file.
#include "cluon-complete.hpp"

// Include our self-contained message definitions.
#include "example.hpp"
#include "opendlv-standard-message-set.hpp"

// We will use iostream to print the received message.
#include <iostream>

#include <string>
#include <fstream>


void writeMessageToFile(std::ofstream &fd, cluon::data::Envelope env){
  std::string data = cluon::serializeEnvelope(std::move(env));

  fd.write(&data[0], data.length());
}

int main(int argc, char **argv) {

  double start_time_relative, time_interval;

  /*if (argc < 2) {
    std::cerr << "Use: " << argv[0] << " FILENAME [START_TIME] [TIME_INTERVAL]" << std::endl;
    exit(-1);
  }
  else if(argc == 2){
    time_interval = 5.0;
    start_time_relative = 0.0;
  }
  else {
    std::cerr << "Use: " << argv[0] << " FILENAME [START_TIME] [TIME_INTERVAL]" << std::endl;
    exit(-1);
  }*/

  time_interval = 5.0;
  start_time_relative = 0.0;

  std::string filename_out = "example_out.rec";
  std::ofstream f_out(filename_out, std::ios::out | std::ios::binary);

  if (!f_out){
    std::cout << "Cannot open output file! " << std::endl;
    exit(-1);
  }

  bool firstEnvelope = true;
  double timestamp, start_time, stop_time;
  std::string filename_in = "test.rec";

  // We will use cluon::Player to parse and process a .rec file.
  constexpr bool AUTO_REWIND{false};
  constexpr bool THREADING{false};
  //cluon::Player player(std::string(argv[1]), AUTO_REWIND, THREADING);
  cluon::Player player(filename_in, AUTO_REWIND, THREADING);

  // Now, we simply loop over the entries.
  while (player.hasMoreData()) {
    auto entry = player.getNextEnvelopeToBeReplayed();
    // The .first field indicates whether we have a valid entry.
    if (entry.first) {
      // Get the Envelope with the meta-information.
      cluon::data::Envelope env = entry.second;

      // Get timestamp
      timestamp = env.sampleTimeStamp().seconds() + 1e-6*env.sampleTimeStamp().microseconds();

      // Set timestamp of first envelope to filter start time
      if (firstEnvelope){

        start_time = timestamp + start_time_relative;
        stop_time = start_time + time_interval;

        firstEnvelope = false;
      }
/*
      // Check whether it is of type GeodeticWgs84Reading.
      if (env.dataType() == opendlv::proxy::GeodeticWgs84Reading::ID()) {
        // Unpack the content...
        auto msg = cluon::extractMessage<opendlv::proxy::GeodeticWgs84Reading>(std::move(env));

      }

      // Check whether it is of type TestMessage2.
      if (env.dataType() == odcore::testdata::TestMessage2::ID()) {
        // Unpack the content...
        auto msg = cluon::extractMessage<odcore::testdata::TestMessage2>(std::move(env));

        // ...and traverse its data fields using a lambda function as alternative.
        msg.accept([](uint32_t, const std::string &, const std::string &) {},
                   [](uint32_t, std::string &&, std::string &&n, auto v) { std::cout << n << " = " << +v << std::endl; },
                   [](){});
      }

      // Check whether it is of type TestMessage5.
      if (env.dataType() == odcore::testdata::TestMessage5::ID()) {
        auto msg = cluon::extractMessage<odcore::testdata::TestMessage5>(std::move(env));
        msg.accept([](uint32_t, const std::string &, const std::string &) {},
                   [](uint32_t, std::string &&, std::string &&n, auto v) { std::cout << n << " = " << v << std::endl; },
                   [](){});
      } */

      if (timestamp >= start_time && timestamp <= stop_time){
        writeMessageToFile(f_out, env);
      }
      else if (timestamp > stop_time){
        f_out.close();
        break;
      }

    }
  }

  return 0;
}
