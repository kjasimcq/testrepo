# Quarklink Getting Started

This guide shows how to to setup a secure IoT device with Quarklink.

You will need an ESP32, such as C3 or S3 or M5 Edukit, and a Quarklink instance.

### Note: 
After you follow these steps the device is permanently configured to use secure boot. There is no going back to being an unsecure device and the device can only be updated with firmware signed by the key you have defined in the Quarklink HSM. 

Follow these steps to set up your device with secure boot, encrypted flash memory, and for signed firmware updates.

1. Create a Provisioning Task

The Provisioning Task defines the configuration for a secure device including the boot loader details and the initial firmware which configures the device with its Root-of-Trust and provisions it with its Quarklink details.

2. Run the Provisioning Task

Running the Provision Task connects to an actual device and configures it to be secure.    

3. Upload your Firmware to Quarklink and associate it with a batch of devices

Thats it!

## First sign up for a Quarklink instance and log on

Go to the [Quarklink Signup page](https://signup.quarklink.io] to create your Quarklink instance and then logon to your instance:

![image](https://github.com/cryptoquantique/quarklink-getting-started/assets/12925578/b9ecc6a0-79a2-4b55-a070-ba1703e99008 | width=100px)

![image](https://github.com/cryptoquantique/quarklink-getting-started/assets/12925578/55add17d-518b-4fa6-9e5d-455a9456c8f3)

![image](https://github.com/cryptoquantique/quarklink-getting-started/assets/12925578/509499d9-5c94-4b67-bc33-475a3342c527)

## Create a Provisioning Task

![image](https://github.com/cryptoquantique/quarklink-getting-started/assets/12925578/8ff836ed-704e-4b2f-9451-72c9cf99c0e9)

![image](https://github.com/cryptoquantique/quarklink-getting-started/assets/12925578/29e8d0ab-edbf-437d-904c-5bb77431b108)

![image](https://github.com/cryptoquantique/quarklink-getting-started/assets/12925578/dc051ceb-f1ab-47a6-8151-fa5ab1ef6be2)

## Run the Provisioning Task

![image](https://github.com/cryptoquantique/quarklink-getting-started/assets/12925578/08466349-eaec-40a7-b601-29a5aea0f7bf)

![image](https://github.com/cryptoquantique/quarklink-getting-started/assets/12925578/5231284b-6d51-47e2-aaee-9c68b03448b9)

![image](https://github.com/cryptoquantique/quarklink-getting-started/assets/12925578/3949ef04-1f97-4242-b8b4-8e26d0463aa5)


## Upload your Firmware to Quarklink

![image](https://github.com/cryptoquantique/quarklink-getting-started/assets/12925578/2cae7919-5d76-474e-8899-24da43d12c14)

![image](https://github.com/cryptoquantique/quarklink-getting-started/assets/12925578/089c620f-2d08-4d24-b2db-8964381f233e)

## Associate the firmware to the device batch

![image](https://github.com/cryptoquantique/quarklink-getting-started/assets/12925578/427e2f8a-ace9-4b17-b673-0885ee332b9f)

![image](https://github.com/cryptoquantique/quarklink-getting-started/assets/12925578/b964dee2-f044-4f4c-a206-dbf447617cac)

![image](https://github.com/cryptoquantique/quarklink-getting-started/assets/12925578/0e42beb9-b4a4-494d-bb71-7e9b5322c3af)

## Thats it!

![image](https://github.com/cryptoquantique/quarklink-getting-started/assets/12925578/65d749ef-0e78-496c-9c49-36345ccce55b)
