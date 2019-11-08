#!/usr/bin/python3

import sys
from os import listdir
import struct

# Default messages from libcluon providing Envelope and TimeStamp
import cluonDataStructures_pb2
# OpenDLV Standard Message Set
import opendlv_standard_message_set_v0_9_9_pb2
# Messages from individual message set
import example_pb2
# Parse arguments from command line
import argparse

parser = argparse.ArgumentParser(description='Filter a rec file based on time')
parser.add_argument('-r', metavar='recording', default='test.rec')
parser.add_argument('-s', metavar='start_time', default=0)
parser.add_argument('-i', metavar='time_interval', default=1)
args = parser.parse_args()

################################################################################
# Extract and print an Envelope's payload based on it's dataType.
def extractAndPrintPayload(dataType, payload):
    # Example on how to extract a message from the OpenDLV Standard Message Set:
    if dataType == 19:   # opendlv.proxy.GeodeticWgs84Reading
        messageFromPayload = opendlv_standard_message_set_v0_9_9_pb2.opendlv_proxy_GeodeticWgs84Reading()
        messageFromPayload.ParseFromString(payload)
        print("Payload: %s" % (str(messageFromPayload)))

    # Example on how to extract a message from the individual example message set:
    if dataType == 1002: # TestMessage2
        messageFromPayload = example_pb2.odcore_testdata_TestMessage2()
        messageFromPayload.ParseFromString(payload)
        print("Payload: %s" % (str(messageFromPayload)))

    if dataType == 1005: # TestMessage5
        messageFromPayload = example_pb2.odcore_testdata_TestMessage5()
        messageFromPayload.ParseFromString(payload)
        print("Payload: %s" % (str(messageFromPayload)))


################################################################################
# Print an Envelope's meta information.
def printEnvelope(e):
    print("Envelope ID/senderStamp = %s/%s" % (str(e.dataType), str(e.senderStamp)))
    print(" - sent                 = %s.%s" % (str(e.sent.seconds), str(e.sent.microseconds)))
    print(" - received             = %s.%s" % (str(e.received.seconds), str(e.received.microseconds)))
    print(" - sample time          = %s.%s" % (str(e.sampleTimeStamp.seconds), str(e.sampleTimeStamp.microseconds)))
    extractAndPrintPayload(e.dataType, e.serializedData)
    print()


################################################################################
# Write message to file 
def writeMessageToFile(fd, header, payload):
    fd.write(header)
    fd.write(payload)


################################################################################
# Filter messages in .rec file based on sample timestamp
def filterFile(filename_in, filename_out, start_time_relative, time_window):
    assert filename_in != filename_out, 'Input and output rec files are the same!'
    first_envelope = True

    with open(filename_in, "rb") as f_in:
        with open(filename_out, 'wb') as f_out:
            buf = b""
            bytesRead = 0
            expectedBytes = 0
            LENGTH_ENVELOPE_HEADER = 5
            consumedEnvelopeHeader = False


            byte = f_in.read(1)
            while byte != "":
                buf =  b"".join([buf, byte])
                bytesRead = bytesRead + 1

                if consumedEnvelopeHeader:
                    if len(buf) >= expectedBytes:
                        envelope = cluonDataStructures_pb2.cluon_data_Envelope()
                        envelope.ParseFromString(buf)

                        # Convert timestamp from 2 ints to a float
                        timestamp = envelope.sampleTimeStamp.seconds + 1e-6*envelope.sampleTimeStamp.microseconds

                        # Extract timestamp from first envelope
                        if first_envelope:
                            first_envelope = False
                            start_time = timestamp
                            stop_time = start_time + float(time_window)
                        
                        # Check whether current timestamp is within limits 
                        if timestamp >= start_time+start_time_relative and timestamp <= stop_time:
                            writeMessageToFile(f_out, header, buf)
                        elif timestamp > stop_time:
                            break

                        # Start over and read next container.
                        consumedEnvelopeHeader = False
                        buf = buf[expectedBytes:]
                        expectedBytes = 0

                if not consumedEnvelopeHeader:
                    if len(buf) >= LENGTH_ENVELOPE_HEADER:
                        consumedEnvelopeHeader = True
                        byte0 = buf[0]
                        byte1 = buf[1]

                        # Check for Envelope header.
                        if byte0 == 0x0D and byte1 == 0xA4:
                            v = struct.unpack('<L', buf[1:5]) # Read uint32_t and convert to little endian.
                            expectedBytes = v[0] >> 8 # The second byte belongs to the header of an Envelope.
                            header = buf
                            buf = buf[expectedBytes:] # Remove header.
                        else:
                            print("Failed to consume header from Envelope.")

                # Read next byte.
                byte = f_in.read(1)

                # End processing at file's end.
                if not byte:
                    break


################################################################################
# Does time filtering on all files in a directory
def filterAllFilesInDir(dir_path, start_time_relative, time_window):
    files = listdir(dir_path)
    for e in files:
        name_type = e.split('.')
        
        # Only filter .rec-files
        if len(name_type) == 2 :
            if name_type[1] == 'rec':
                filterFile(e, name_type[0] + '_out.' + name_type[1], start_time_relative,\
                    time_window)

################################################################################
# Main entry point.
# if len(sys.argv) != 2:
    # print("Display Envelopes captured from OpenDLV.")
    # print("  Usage: %s example.rec" % (str(sys.argv[0])))
    # sys.exit()
start_time_relative = float(args.s)
time_window = float(args.i)
filename_in = args.r
if filename_in.split('.') == 2:

    filename_out = filename_in.split('.')[0] + '_out.rec'
    filterFile(filename_in, filename_out, start_time_relative, time_window)

elif filename_in.split('.') < 2:

    filterAllFilesInDir(filename_in, start_time_relative, time_window)