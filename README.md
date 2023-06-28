# QuarkLink Getting Started

Similarly to [quarklink-getting-started](https://github.com/cryptoquantique/quarklink-getting-started), this project is a simple demonstration on how to get started with QuarkLink to secure an esp32 device.

In particular, this project uses the [esp-idf framework](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/) and is designed specifically for the [M5Stack EduKit](https://shop.m5stack.com/products/m5stack-core2-esp32-iot-development-kit-for-aws-iot-edukit) unit, that comes with an on-board Secure Element (Microchip ATECC-608).

The result is an M5Stack (ESP32-Eco3) that is secured by using:
- Secure Boot v2
- Flash Encryption
- Hardware Root-of-Trust

and which can only be updated Over-The-Air with firmware signed by a key from the Quarklink Hardware Security Module (HSM).

See the [Quarklink Getting Started Guide](https://cryptoquantique.github.io/QuarklinkGettingStartedGuide.pdf) for more detailed information.

## Requirements

There are a few requirements needed in order to get started with this project:

- **M5Stack EduKit**
    This project is only meant for this specific device.
- **esp-idf v5.1**
    This project is meant for the esp-idf framework and it needs at least v5.1 due to some updates that are necessary to properly use the secure element.  
    Instructions on how to setup the esp-idf environment can be found [here](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/index.html).
- **quarklink-client libraries**
    The quarklink-client library comes in the form of compiled binaries and can be found [here](https://github.com/cryptoquantique/quarklink-binaries/tree/main/quarklink-getting-started).  
    The files needed for this project are labelled with *m5edukit-ecc608* and need to placed inside the `lib` folder in this project.

## Setup

### Provision the ATECC-608 Secure Element
When configuring a new device, there is need to provision the Secure Element in order to use it.  
The esp-cryptoauthlib component included in this project, is a port of Microchip's [cryptoauthlib](https://github.com/MicrochipTech/cryptoauthlib) for ESP-IDF and includes all the utilities needed to configure, provision and use the Secure Element module. The component can also be found [here](https://github.com/espressif/esp-cryptoauthlib).  
Instructions on how to provision the ATECC-608 are included in the `esp_cryptoauth_utility` [README.md](./components/esp-cryptoauthlib/esp_cryptoauth_utility/README.md).  

### Project configuration
Use ESP-IDF's `idf.py` command to configure the project:
- Run `idf.py set-target esp32` to select the target device (note: M5Stack edukit mounts an esp32 rev3)  
    This will create the file *sdkconfig*.
- There is no need for further configuration, as any value that differs from default is specified in the *sdkconfig.defaults* file.

It is important to leave the configuration as it is, especially the parts concerning the Security Features, Partition Table and Bootloader, as changing these will brick the device.

## Build
Run `idf.py build` to build the project. 
This command will generate the firmware that needs to be uploaded to QuarkLink for the OTA update.
```
idf.py build

. . .

Creating esp32 image...
Merged 25 ELF sections
Successfully created esp32 image.
Generated <path/to/>quarklink-getting-started-m5edukit-ecc608/build/quarklink-getting-started-m5edukit-ecc608.bin

. . .

```

You might notice that at the end the utility prompts you to sign the firmware and suggests what command to run to flash the device. This is not possible, since the device has already been provisioned via QuarkLink and can only be updated with firmware that is signed with the same key, securely stored in the QuarkLink HSM.

In order to update your device, you need to upload the generated binary `build/quarklink-getting-started-m5edukit-ecc608.bin` to your QuarkLink instance, where it will be signed and provided to the running device as an over-the-air update.