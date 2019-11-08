// Include the header-only libcluon file.
#include "cluon-complete.hpp"

// Include our self-contained message definitions.
#include "example.hpp"
#include "opendlv-standard-message-set.hpp"

// We will use iostream to print the received message.
#include <iostream>

#include <string>
#include <fstream>

// Function pointer declaration
void writeMessageToFile(std::ofstream &fd, cluon::data::Envelope);
int fileType(std::string);
int cutDirectory(std::string, double, double);
int cutFile(std::string, double, double);

int main(int argc, char **argv) {

  double start_time_relative, time_interval;
/*
  if (argc < 2) {
    std::cerr << "Use: " << argv[0] << " FILENAME [START_TIME] [TIME_INTERVAL]" << std::endl;
    exit(-1);
  }
  else if(argc == 2){
    // Default values if only filename is given
    time_interval = 5.0;
    start_time_relative = 0.0;
  }
  else {
    std::cerr << "Use: " << argv[0] << " FILENAME [START_TIME] [TIME_INTERVAL]" << std::endl;
    exit(-1);
  }
*/

  time_interval = 5.0;
  start_time_relative = 0.0;

  std::string filename_in = "test.rec";
  int type = fileType(filename_in);

  // If directory, cut all eligible files into the given interval
  if (type == 0){
    cutDirectory(filename_in, start_time_relative, time_interval);
  }
  // If filename_in is a file then cut it
  else if (type == 1){
    cutFile(filename_in, start_time_relative, time_interval);
  }
  else if (type == 2){
    std::cout << "interpreted filename as invalid rec file" << std::endl;
  }
  return 0;
}


// Write envelope data to file
void writeMessageToFile(std::ofstream &fd, cluon::data::Envelope env){
  std::string data = cluon::serializeEnvelope(std::move(env));
  fd.write(&data[0], data.length());
}

// Determines if given filename is a .rec-file or a directory
// Also checks if it is the output from a previous split
// Returns 0 if filename refers to a directory
// 1 if it is a valid .rec-file
// 2 if it is an invalid .rec-file
// NOTE: This function is allergic to filenames that contains the string
// '_rec'
int fileType(std::string filename){
  char *current_token = strtok(&filename[0], "_.");

  if (current_token != NULL){
    while(current_token != NULL){

      std::cout << current_token << std::endl;
      if (strcmp(current_token, "out") == 0){
        return 2;
      }
      else if (strcmp(current_token, "rec") == 0){
        return 1;
      }
      current_token = strtok(NULL, "_.");
    }
  }
  return 0;
}

// Cut file 'filename_in' and put output in 'filename_in_out'
int cutFile(std::string filename_in, double start_time_relative, double time_interval){
  std::string tmpstr;
  strcpy(&tmpstr[0], &filename_in[0]);

  std::string filename_out = strcat(strtok(&tmpstr[0], "."), "_out.rec");
  std::ofstream f_out(filename_out, std::ios::out | std::ios::binary);


  if (!f_out){
    std::cout << "Cannot open output file! " << std::endl;
    exit(-1);
  }

  bool firstEnvelope = true;
  double timestamp, start_time, stop_time;

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

int cutDirectory(std::string dir_name, double start_time_relative, double time_interval){
  std::cout << "NOT IMPLEMENTED" << std::endl;
}