# QuarkLink Database Direct

This example project provides instructions on how to use QuarkLink to make a secure IoT device using an ESP32 and specificly connect to a Database Direct([esp32-c3](https://docs.espressif.com/projects/esp-idf/en/latest/esp32c3/hw-reference/esp32c3/user-guide-devkitm-1.html)).

The result is an ESP32 that is using secure boot, flash encryption, has a Root-of-Trust, and which can only be updated Over-The-Air with firmware signed by a key from the QuarkLink Hardware Security Module (HSM).

See the [QuarkLink User Guide](https://docs.quarklink.io/docs/database-direct) for more detailed information on how to create a Database Direct connection.

In case you want to use MongoDB as Database Direct solution, check out the [Mongo Atlas QuarkLink Setup Procedure](https://docs.quarklink.io/docs/mongo-atlas-quarklink-setup-procedure) for more detailed information on how to setup MongoDB Atlas

## Requirements

There are a few requirements needed in order to get started with this project:

- **PlatformIO**
    The project uses the PlatformIO build environment. If you don't already have this follow the installation instructions [here](https://platformio.org/install).  
    You can verify PlatformIO is installed with ```pio --version``` command:
    ```sh
    >pio --version
    PlatformIO Core, version 6.1.7
    ``` 
- **quarklink-client libraries**
    The quarklink-client library comes in the form of compiled binaries and can be found in the [quarklink-binaries repository](https://github.com/cryptoquantique/quarklink-binaries/tree/main/quarklink-client).  
    Copy the required files into the `lib` folder of this project.  
    For example, if building for `esp32-c3`: copy the file `libquarklink-client-esp32-c3-ds-v1.4.0.a` to the local clone of this repository, inside `lib`.

## Pre-built binaries

There are pre-built binaries of this project available in the [quarklink-binaries repository](https://github.com/cryptoquantique/quarklink-binaries/tree/main/quarklink-examples/database-direct).  
These binaries can be programmed into the ESP32 device using the QuarkLink provisioning facility. No need for third party programming tools.

## Building this project

To build the project use the ```pio run``` command:
```sh
> pio run
Processing esp32-c3-ds-vefuse (board: esp32-c3-devkitm-1; platform: espressif32 @6.4.0; framework: espidf)
------------------------------------------------------------------------------------------------------------------------------Verbose mode can be enabled via `-v, --verbose` option
...
Patch file: ota_ds_peripheral.patch
Patch has already been applied
CONFIGURATION: https://docs.platformio.org/page/boards/espressif32/esp32-c3-devkitm-1.html
PLATFORM: Espressif 32 (6.4.0) > Espressif ESP32-C3-DevKitM-1
HARDWARE: ESP32C3 160MHz, 320KB RAM, 4MB Flash
...
Building in release mode
Retrieving maximum program size .pio\build\esp32-c3-ds-vefuse\firmware.elf
Checking size .pio\build\esp32-c3-ds-vefuse\firmware.elf
Advanced Memory Usage is available via "PlatformIO Home > Project Inspect"
RAM:   [==        ]  16.5% (used 54072 bytes from 327680 bytes)
Flash: [========= ]  92.3% (used 967932 bytes from 1048576 bytes)
================================================ [SUCCESS] Took 7.56 seconds ================================================

Environment         Status    Duration
------------------  --------  ------------
esp32-c3-ds-vefuse  SUCCESS   00:00:07.565
================================================ 1 succeeded in 00:00:07.565 ================================================ 

```

The build will create a firmware binary file within the .pio directory:
```sh
>dir /b .pio\build\esp32-c3-ds-vefuse\firmware.bin
firmware.bin
```

The ```firmware.bin``` file is what you upload to QuarkLink. Click on the "Firmwares" option of the QuarkLink main menu to access the uploading function.  
Once uploaded to QuarkLink configure your Batch with the new firmware image and it will be automatically downloaded to the ESP32.  
See the [QuarkLink Getting Started Guide](https://docs.quarklink.io/docs/getting-started-with-quarklink-ignite#getting-started) for more details.

## Configurations
There are currently two configurations available for the firmware:
- virtual-efuses
- release

When building, make sure to choose the same configuration that the device was provisioned with via QuarkLink.

The default environment is `esp32-c3-ds-vefuses`, as can be seen from the [example above](#building-this-project). To build the firmware with the `esp32-c3-ds-release` configuration, run the command 
```sh
>pio run -e esp32-c3-release
```

>**Note:** It is assumed that the user has already familiarised with the difference between *virtual-efuses* and *release* during the QuarkLink provisioning process.  
More information on what virtual efuses are can be found as part of [espressif programming guide](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/efuse.html#virtual-efuses).

### Debug
All the environments can be updated to use the debug version of the QuarkLink client library if needed. In order to do this, open the file [platformio.ini](platformio.ini) and update the desired configuration `build_flags` option.  
Example:
```ini
[env:esp32-c3-ds-release]
board = esp32-c3-devkitm-1
build_flags = -Llib -lquarklink-client-esp32-c3-ds-v1.4.0 -Iinclude
```
Becomes:
```ini
[env:esp32-c3-ds-release]
board = esp32-c3-devkitm-1
build_flags = -Llib -lquarklink-client-esp32-c3-ds-v1.3.0-debug -Iinclude
```

## Building project version with RGB LED
This is currently supported only for the esp32-c3 board.

To enable the use of the RGB LED, you need to configure its colour before building the project with ```pio run```. This is done by setting the following environment variable:
```PLATFORMIO_BUILD_FLAGS="-DLED_COLOUR=<COLOUR>"```

On Windows systems this is achieved as follows (e.g. green LED):
```sh
>$Env:PLATFORMIO_BUILD_FLAGS="-DLED_COLOUR=GREEN"
```
Whereas on Unix systems the analogous command is:
```sh
export PLATFORMIO_BUILD_FLAGS="-DLED_COLOUR=GREEN"
``` 

>**Note:** Currently supported colours are: GREEN and BLUE

To reset the LED configuration and disable the LED simply reset the variable:
```sh
>$Env:PLATFORMIO_BUILD_FLAGS=""
```

The device will need to be un-plugged from power for the change to take effect.

## Further Notes
**Custom Partition Table:** users might be interested in using their own partition table with QuarkLink. Currently, support for this feature is only for paid tiers, however users are welcome to request a custom partition table via the GitHub issues on this project.
