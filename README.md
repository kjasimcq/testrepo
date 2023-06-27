# quarklink-getting-started

This project is a simple demo how to use quarklink-client binary files.

### Project Flow

Download the latest Build Artifacts folder from  [esp32-ql-binary-generator pipelines](https://dev-gitlab.cryptoquantique.com/firmware-development/esp-idf/esp32-ql-binary-generator/pipelines).
From the quarklinkbins folder copy the .a files to the [lib](lib) folder of this project.

The header file [quarklink.h](include/quarklink.h) has already been copied from quarklink-client libary.

Before to build the esp32-ql-binary-simple-demo project, in the [platformio.ino](platformio.ino) file, at line 3, check for which environment you want to run this demo: esp32-m5edukit, esp32-m5edukit-debug, esp32-c3 or, esp32-c3-debug.
