/***
 * This example expects the serial port has a loopback on it.
 *
 * Alternatively, you could use an Arduino:
 *
 * <pre>
 *  void setup() {
 *    Serial.begin(<insert your baudrate here>);
 *  }
 *
 *  void loop() {
 *    if (Serial.available()) {
 *      Serial.write(Serial.read());
 *    }
 *  }
 * </pre>
 */

#include <string>
#include <chrono>
#include <fstream>
#include <iostream>
#include <cstdio>
#include <signal.h>

// OS Specific sleep
#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

#include "serial/serial.h"
#include "/home/zhenyu/smt/OAPI-Bot/linux_curl/cpp_program/include/test.hpp"

using std::string;
using std::exception;
using std::cout;
using std::cerr;
using std::endl;
using std::vector;
    
std::ofstream myfile;

void signal_callback_handler(int signum) {
    cout << "Caught signal " << signum << endl;
    // Terminate program
    myfile.close();
    exit(signum);
}

struct DataFormat
{
    bool is_data_good = false;
    long unix_time = -1;
    double light = -1;
    double soundPressure = -1;
    double motionX = -1;
    double motionY = -1;
    double motionZ = -1;
    double temperature = -1;

    DataFormat(string data_str, long unix_time_in)
    {
        size_t pos = 0;
        std::string token;
        std::string delimiter = ",";
        std::vector<double> data_vec;
        while ((pos = data_str.find(delimiter)) != std::string::npos) 
        {
            token = data_str.substr(0, pos);
            //std::cout << token << std::endl;
            data_vec.push_back(std::stod(token));
            data_str.erase(0, pos + delimiter.length());
        }
        data_vec.push_back(std::stod(data_str));

        //printv(data_vec.size());
        if(data_vec.size() == 6)
        {
            unix_time = unix_time_in;
            is_data_good = true;
            light         = data_vec[0];
            soundPressure = data_vec[1];
            motionX       = data_vec[2];
            motionY       = data_vec[3];
            motionZ       = data_vec[4];
            temperature   = data_vec[5];
        }
    }

    std::string str()
    {
        std::stringstream ss;
        if(is_data_good)
        {
            ss<<"unix_time: "<<unix_time<<", light: "<<light<<", soundPressure: "<<soundPressure<<", acc: {"<<motionX<<","<<motionY<<","<<motionZ<<"}, temperature: "<<temperature;
        }
        else
        {
            ss<<"BadData...";
        }
        return ss.str();
    }
    
    std::string strLite()
    {
        std::stringstream ss;
        ss<<unix_time<<","<<light<<","<<soundPressure<<","<<motionX<<","<<motionY<<","<<motionZ<<","<<temperature;
        return ss.str();
    }
};

void my_sleep(unsigned long milliseconds) {
#ifdef _WIN32
    Sleep(milliseconds); // 100 ms
#else
    usleep(milliseconds*1000); // 100 ms
#endif
}

void enumerate_ports()
{
    vector<serial::PortInfo> devices_found = serial::list_ports();

    vector<serial::PortInfo>::iterator iter = devices_found.begin();

    while( iter != devices_found.end() )
    {
        serial::PortInfo device = *iter++;

        printf( "(%s, %s, %s)\n", device.port.c_str(), device.description.c_str(),
                device.hardware_id.c_str() );
    }
}

void print_usage()
{
    cerr << "Usage: test_serial {-e|<serial port address>} ";
    cerr << "<baudrate> [test string]" << endl;
}

int run(int argc, char **argv)
{
    if(argc < 2) {
        print_usage();
        return 0;
    }

    // Argument 1 is the serial port or enumerate flag
    string port(argv[1]);

    if( port == "-e" ) {
        enumerate_ports();
        return 0;
    }
    else if( argc < 3 ) {
        print_usage();
        return 1;
    }

    // Argument 2 is the baudrate
    unsigned long baud = 0;
#if defined(WIN32) && !defined(__MINGW32__)
    sscanf_s(argv[2], "%lu", &baud);
#else
    sscanf(argv[2], "%lu", &baud);
#endif

    // port, baudrate, timeout in milliseconds
    serial::Serial my_serial(port, baud, serial::Timeout::simpleTimeout(10));

    cout << "Is the serial port open?";
    if(my_serial.isOpen())
        cout << " Yes." << endl;
    else
        cout << " No." << endl;

    // Get the Test string
    int count = 0;
    string test_string;
    if (argc == 4) {
        test_string = argv[3];
    } else {
        test_string = "Testing.";
    }

    // Test the timeout, there should be 1 second between prints
    cout << "Timeout == 1000ms, asking for 1 more byte than written." << endl;
    myfile.open ("arduino_data.txt");
    std::string buffer;
    std::string temp_data_str;
    while (true) {

        string result = my_serial.read(1000);

        buffer = buffer + result;

        //cout << "Iteration: " << count << ", Bytes written: ";
        //cout << result.length() << ", String read: " << result << endl;
        int first_occur = buffer.find('\n');
        if(first_occur > 0)
        {
            // if find \n, take the data from buffer and parse it
            temp_data_str = buffer.substr (0,first_occur);
            printv(temp_data_str);
            long microsecondsUTC = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
            printv(microsecondsUTC);
            DataFormat df(temp_data_str,microsecondsUTC);
            printv(df.str());
            if(df.is_data_good)
            {
                printv(df.strLite());
                myfile << df.strLite();
                myfile << "\n";
            }

            buffer = buffer.substr(first_occur+1);
            //printv(buffer);
        }
        //printv(first_occur);

        count += 1;
    }
    myfile.close();
    return 0;
}

int main(int argc, char **argv) {
    // Register signal and signal handler
    signal(SIGINT, signal_callback_handler);
    try {
        return run(argc, argv);
    } catch (exception &e) {
        cerr << "Unhandled Exception: " << e.what() << endl;
    }
}
