---
platform: Free RTOS
device: Wio Terminal
language: C++
---

Connect Accumulation Counter (Ultrasonic) for Wio Terminal device to your Azure IoT services
===

---
# Table of Contents

-   [Introduction](#Introduction)
-   [Bill of Materials](#BillofMaterials)
-   [Supported Device Attestation Methods](#SupportedDeviceAttestationMethods)
-   [Prerequisites](#Prerequisites)
-   [Prepare the Device](#preparethedevice)
-   [Connect to Azure IoT](#ConnecttoAzureIoT)
-   [Additional Links](#AdditionalLinks)

<a name="Introduction"></a>
# Introduction 

This document describes how to connect Accumulation Counter (Ultrasonic) for Wio Terminal to Azure IoT Hub using the Azure IoT Explorer with certified device application and device models.

IoT Plug and Play certified device simplifies the process of building devices without custom device code. Using Solution builders can be integrated quickly using the certified IoT Plug and Play enabled device based on Azure IoT Central as well as third-party solutions.

This getting started guide provides step by step instruction on getting the device provisioned to Azure IoT Hub using Device Provisioning Service (DPS) and using Azure IoT Explorer to interact with device's capabilities.

The Accumulation Counter (Ultrasonic) for Wio Terminal is a device that counts the passage or approach of an object.
It constantly measures the distance with an ultrasonic sensor and counts up when it falls below a threshold.
And the count is then sent to Azure IoT.
By using with Azure, monitoring of equipment operation and detection of abnormalities, etc can be realized.

<img src="media/3-1.jpg" height="250"/>
<img src="media/3-2.png" height="250"/>

<a name="BillofMaterials"></a>
# Bill of Materials

|Name|SKU#|Quantity|Wiki|Shop|
|:--|:--|:--|:--|:--|
|Wio Terminal|102991299|1|[here](https://wiki.seeedstudio.com/Wio-Terminal-Getting-Started/)|[here](https://www.seeedstudio.com/Wio-Terminal-p-4509.html)|
|Grove - Ultrasonic Distance Sensor|101020010|1|[here](http://wiki.seeedstudio.com/Grove-Ultrasonic_Ranger/)|[here](https://www.seeedstudio.com/Grove-Ultrasonic-Distance-Sensor.html)|

<a name="SupportedDeviceAttestationMethods"></a>
# Supported Device Attestation Methods

The following table summarizes supported device attestation/authentication methods :

| Service           | Enrollment | Authentication           | Support status |
|:------------------|:-----------|:-------------------------|:---------------|
| Azure IoT Hub     | -          | Symmetric Key            | Need recompile |
| Azure IoT Hub     | -          | X.509 Self-Signed        | Not Supported  |
| Azure IoT Hub     | -          | X.509 CA-Signed          | Not Supported  |
| Azure IoT Hub DPS | Group      | Symmetric Key            | **Supported**  |
| Azure IoT Hub DPS | Group      | CA Certificate           | Not Supported  |
| Azure IoT Hub DPS | Group      | Intermediate Certificate | Not Supported  |
| Azure IoT Hub DPS | Individual | Symmetric Key            | **Supported**  |
| Azure IoT Hub DPS | Individual | X.509                    | Not Supported  |
| Azure IoT Hub DPS | Individual | TPM                      | Not Supported  |
| Azure IoT Central | Group      | Symmetric Key            | **Supported**  |
| Azure IoT Central | Group      | CA Certificate           | Not Supported  |

<a name="Prerequisites"></a>
# Prerequisites

You should have the following items ready before beginning the process:

**For Azure IoT Central**
-   [Azure IoT Central application](https://docs.microsoft.com/en-us/azure/iot-central/core/overview-iot-central)

**For Azure IoT Hub**
-   [Azure IoT Hub instance](https://docs.microsoft.com/en-us/azure/iot-hub/about-iot-hub)
-   [Azure IoT Hub Device Provisioning Service](https://docs.microsoft.com/en-us/azure/iot-dps/quick-setup-auto-provision)

<a name="preparethedevice"></a>
# Prepare the Device

In order to configure WiFi and Azure IoT device authentication, please install a serial terminal application such as Putty.
Wio Terminal uses a serial console command line tool to configure network and Azure IoT provisioning information.

[A game application](https://wiki.seeedstudio.com/Wio-Terminal-Firmware/) is pre-installed to Wio Terminal.  
In order to connect to Azure IoT, please follow steps below :

1. Assemble
2. Update the Wi-Fi Firmware
3. Install Azure IoT Firmware
4. Boot Wio Terminal into the configuration mode
5. Configuring Wi-Fi
6. Reboot Wio Terminal

## 1. Assemble

Connect the Grove - Ultrasonic Distance Sensor to the Wio Terminal with the Grove cable.
The connection to the Wio Terminal is the right side Grove connector.

<img src="media/3-3.jpg" width="400"/>

## 2. Update the Wi-Fi Firmware

Please follow instruction at [Wio Terminal's network overview page](https://wiki.seeedstudio.com/Wio-Terminal-Network-Overview/#update-the-wireless-core-firmware) to update the firmware.

> You must use the version of Wi-Fi firmware specified in Azure IoT Firmware.
> See [the release page](https://github.com/SeeedJP/wioterminal-iotpnp-device/releases).

## 3. Install Azure IoT Firmware

1. Download the latest firmware **wioterminal-accumulation-counter-ultrasonic.uf2** from [the release page](https://github.com/SeeedJP/wioterminal-iotpnp-device/releases)
2. Connect Wio Terminal to your PC
3. Slide the power switch twice quickly to enter bootloader mode ([Wio Terminal FAQ](https://wiki.seeedstudio.com/Wio-Terminal-Getting-Started/#faq))
4. Confirm the Wio Terminal appears as a storage device in your PC as "Arduino"
5. Copy **wioterminal-accumulation-counter-ultrasonic.uf2** to the **Arduino** drive

## 4. Boot Wio Terminal into the configuration mode

1. Turn on Wio Terminal while holding 3 buttons on the top of the device
2. Confirm LCD displays `In configuration mode`

## 5. Configuring Wi-Fi

1. Start the serial terminal application and configure the serial port number and baudrate
2. In the serial terminal application, type `help`  

    Output example:

    ```
    # help
    Configuration console:
    - help: Help document.
    - reset_factory_settings: Reset factory settings.
    - show_settings: Display settings.
    - set_wifissid: Set Wi-Fi SSID.
    - set_wifipwd: Set Wi-Fi password.
    - set_az_idscope: Set id scope of Azure IoT DPS.
    - set_az_regid: Set registration id of Azure IoT DPS.
    - set_az_symkey: Set symmetric key of Azure IoT DPS.
    - set_az_iotc: Set connection information of Azure IoT Central.

    #
    ```

3. Enter Wi-Fi SSID and password with `set_wifissid` and `set_wifipwd` commands

    Output example:

    ```
    # set_wifissid SSID
    Set Wi-Fi SSID successfully.

    # set_wifipwd PASSWORD
    Set Wi-Fi password successfully.

    #
    ```

## 6. Reboot Wio Terminal

Slide power switch to reset Wio Terminal.

<a name="ConnecttoAzureIoT"></a>
# Connect to Azure IoT

Depending on the authentication you would like to use, configure device authentication using set_az* command in the configuration mode.

## For Azure IoT Central

### 1. Booting into the configuration mode

1. Turn on Wio Terminal while holding 3 buttons on the top of the device
2. Confirm LCD displays `In configuration mode`

### 2. Execute set_az_iotc command

```
# set_az_iotc IDSCOPE SYMKEY DEVICEID
Set connection information of Azure IoT Central successfully.

#
```

<img src="media/1.png" width="800"/>

### 3. Reboot Wio Terminal

Slide power switch to reset Wio Terminal.

## For Azure IoT Hub DPS using individual enrollment


### 1. Booting into the configuration mode

1. Turn on Wio Terminal while holding 3 buttons on the top of the device
2. Confirm LCD displays `In configuration mode`

### 2. Execute set_az_iotc command

```
# set_az_idscope IDSCOPE
Set id scope successfully.

# set_az_regid REGID
Set registration id successfully.

# set_az_symkey SYMKEY
Set symmetric key successfully.

#
```

### 3. Reboot Wio Terminal

Slide power switch to reset Wio Terminal.

<a name="AdditionalLinks"></a>
# Additional Links

- [Source code (on GitHub)](https://github.com/SeeedJP/wioterminal-iotpnp-device/releases)

Please refer to the below link for additional information for Plug and Play 

-   [Manage cloud device messaging with Azure-IoT-Explorer](https://github.com/Azure/azure-iot-explorer/releases)
-   [Import the Plug and Play model](https://docs.microsoft.com/en-us/azure/iot-pnp/concepts-model-repository)
-   [Configure to connect to IoT Hub](https://docs.microsoft.com/en-us/azure/iot-pnp/quickstart-connect-device-c)
-   [How to use IoT Explorer to interact with the device ](https://docs.microsoft.com/en-us/azure/iot-pnp/howto-use-iot-explorer#install-azure-iot-explorer)   
