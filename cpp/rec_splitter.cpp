// Include the header-only libcluon file.
#include "cluon-complete.hpp"

// Include our self-contained message definitions.
#include "example.hpp"
#include "opendlv-standard-message-set.hpp"

// We will use iostream to print the received message.
#include <iostream>

#include <string>
#include <fstream>
#include <dirent.h>

#include <sys/types.h>
#include <sys/stat.h>

// Function prototypes 
void writeMessageToFile(std::ofstream &fd, cluon::data::Envelope);
int fileType(std::string);
int cutDirectory(std::string, double, double);
int cutFile(std::string, double, double);
bool updateGPXString(std::string&, cluon::data::Envelope, double, bool);

int main(int argc, char **argv) {

  double start_time_relative, time_interval;

  if (argc < 2) {
    std::cerr << "Use: " << argv[0] << " FILENAME [START_TIME] [TIME_INTERVAL]" << std::endl;
    exit(-1);
  }
  else if(argc == 2){
    // Default values if only filename is given
    time_interval = 5.0;
    start_time_relative = 100.0;
  }

  // Non-error tolerant argument parsing
  else if(argc > 2 && argc < 5){
    start_time_relative = std::stod(argv[2]);
    time_interval = std::stod(argv[3]);
  }
  else {
    std::cerr << "Use: " << argv[0] << " FILENAME [START_TIME] [TIME_INTERVAL]" << std::endl;
    exit(-1);
  }

  //time_interval = 5.0;
  //start_time_relative = 0.0;

  std::string filename_in = argv[1];
  //std::string filename_in = "testdir";
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

bool updateGPXString(std::string& gpxString, cluon::data::Envelope env, double timestamp, std::string file, bool firstGPS) {
  
  // Unpack the content...
  auto msg = cluon::extractMessage<opendlv::logic::sensation::Geolocation>(std::move(env));

  // ...and access the data fields.
  // Create GPX file.
  // Change into timestamp into ISO8601
  time_t curr_time = (time_t)timestamp;
  char buf[sizeof "2011-10-08T07:07:09Z"];
  strftime(buf, sizeof buf, "%FT%TZ", gmtime(&curr_time));

  // Fetch geolocations
  std::string lat = std::to_string(msg.latitude());
  std::string lon = std::to_string(msg.longitude());
  std::string ele = std::to_string(msg.altitude());

  // Build first input into GPX file.
  // example from:  
  // https://wiki.openstreetmap.org/wiki/GPX
  if (firstGPS){
    gpxString += "<?xml version=\"1.0\" encoding=\"UTF-8\"?> \n";
    gpxString += "<gpx version=\"1.0\"> \n";
    gpxString += "  <name>" + file + "</name>\n";
    gpxString += "  <trk><name>" + file + "</name><number>1</number><trkseg>\n";
    
    firstGPS = false;
  }
  
  gpxString += "    <trkpt lat=\"" + lat + "\" lon=\"" + lon + "\"><ele>" + ele + "</ele><time>" + buf + "</time></trkpt>\n";

  return firstGPS;
}
// Cut file 'filename_in' and put output in 'filename_in_out'
int cutFile(std::string filename_in, double start_time_relative, double time_interval){
  std::string tmpstr;
  tmpstr += filename_in;

  
  //std::cout << filename_in << std::endl;

  std::size_t found = filename_in.find_last_of("/\\");
  std::string path =  filename_in.substr(0,found);
  std::string file = filename_in.substr(found+1);
  std::string file_out = strcat(strtok(&file[0], "."), "_out.rec");
  std::string filename_out = path + "_out/" + file_out;

  std::ofstream f_out(filename_out, std::ios::out | std::ios::binary);



  if (!f_out){
    std::cout << "Cannot open output file! " << std::endl;
    exit(-1);
  }

  bool firstEnvelope = true;
  bool firstGPS = true;
  double timestamp, start_time, stop_time;

  // We will use cluon::Player to parse and process a .rec file.
  constexpr bool AUTO_REWIND{false};
  constexpr bool THREADING{false};
  //cluon::Player player(std::string(argv[1]), AUTO_REWIND, THREADING);
  cluon::Player player(filename_in, AUTO_REWIND, THREADING);
  int i = 0;
  std::string gpxString = "";
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

        // Check whether it is of type GeodeticWgs84Reading.
        if (timestamp >= start_time + i*time_interval/100) {
          if (env.dataType() == opendlv::logic::sensation::Geolocation::ID()) {
            firstGPS = updateGPXString(gpxString, env, timestamp, file, firstGPS);
            i++;
          }
        }

        writeMessageToFile(f_out, env);
      }
      else if (timestamp > stop_time){
        break;
      }

    }
  }
  if (firstGPS == false){
    gpxString += "  </trkseg></trk>\n";
    gpxString += "</gpx>";
    std::string gps_out = path + "_out/gps.gpx";
    std::ofstream f_out2(gps_out, std::ios::out | std::ios::binary);
    f_out2.write(&gpxString[0], gpxString.length());
    f_out2.close();
  }
  f_out.close();
  return 0;
}


int cutDirectory(std::string dir_name, double start_time_relative, double time_interval){
  std::string tmpstr = "";
  tmpstr += dir_name;
  tmpstr += '/';
  std::string newFolder = "";
  newFolder += dir_name;
  newFolder += "_out/";
  if (mkdir(newFolder.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH) == -1)
  {
      if( errno == EEXIST ) {
        // alredy exists
      } else {
        // something else
          std::cout << "cannot create sessionnamefolder error:" << strerror(errno) << std::endl;
      }
  }
  
  if (auto dir = opendir(&dir_name[0])){
    while (auto f = readdir(dir)){

      if (fileType(f->d_name) == 1){
        //std::cout << strcat(&tmpstr[0],f->d_name) << std::endl;
        cutFile(tmpstr + f->d_name, start_time_relative, time_interval);
      }
    }
  }
}