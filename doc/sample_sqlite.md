# SQLite Sample for Azure IoT Gateway SDK #

## Overview ##

This sample showcases how one can build an IoT gateway that interacts with 
SQLite databases using the Azure IoT Gateway SDK. The sample contains the following modules:

  1. A Modbus module that interfaces with a Modbus TCP/RTU device to
     read data.
  2. A SQLite module that interfaces with SQLite databases.
  3. An identity mapping module for translating between device MAC addresses
     and Azure IoT Hub device identities.
  4. An IoT Hub module for uploading result of SQLite commands and for receiving
     SQLite commands from the Azure IoT Hub.

##  How the data flows through the Gateway ##

A piece of telemetry data generated from the Modbus device gets `INSERT` or `UPDATE` into the SQLite database:

![](./media/gateway_sqlite_source_data_flow.png)

  1. The Modbus device generates a data set and transfers it over
     Modbus TCP/RTU to the Modbus module.
  2. The Modbus module receives the data and construct the SQLite command to do `INSERT` or `UPDATE`. The SQLite command is then published on to the message broker.
  3. The SQLite module picks up this message from the message broker and looks up its source id to verify it is a valid source.
  4. The SQLite module then picks up this message and executes the SQLite command 
     operation by calling SQLite3 APIs.

The cloud to device command data flow pipeline is described via a block diagram
below:

![](./media/gateway_sqlite_cloud_data_flow_1.png)

  1. The IoT Hub module receives a new command message from IoT Hub and it
     publishes it to the message broker.
  2. The Identity Mapping module picks up the message and translates the Azure
     IoT Hub device ID to a device MAC address and publishes a new message to
     the message broker including the MAC address in the message's properties map.
  3. The SQLite module then picks up this message and executes the SQLite command 
     operation by calling SQLite3 APIs.

The device to cloud command result data flow pipeline is described via a block diagram
below:

![](./media/gateway_sqlite_cloud_data_flow_2.png)

  1. The SQLite module receives the result of command execution from SQLite3 APIs. 
     The result is then published on to the message broker along with the device's MAC address.
  2. The identity mapping module picks up this message from the message broker and
     looks up its internal table in order to translate the device MAC address 
     into an Azure IoT Hub identity (comprised of a device ID and device key). 
     It then proceeds to publish a new message on to the message broker containing 
     the sample data, the MAC address, the IoT Hub device ID and
     key.
  3. The IoT Hub module then receives this message from the identity
     mapping module and publishes it to the Azure IoT Hub itself.

## Building the sample ##

The [devbox setup](./devbox_setup.md) guide has information on how you can build the
module.

## Preparing your Modbus module ##

The instructions to setup a Modbus module can be found [here](https://github.com/Azure/iot-gateway-modbus).

## Running the sample ##

In order to bootstrap and run the sample, you'll need to configure each module
that participates in the gateway. This configuration is provided as JSON. All
4 participating modules will need to be configured. There are sample JSON files
provided in the repo called `sqlite_win.json` and `sqlite_win.json` which you can 
use as a starting point for building your own configuration file. You should find 
the file at the path `samples/modbus/src` relative to the root of the repo.

In order to run the sample you'll run the `modbus_sample` binary passing the
path to the configuration JSON file.

```
modbus_sample <<path to the configuration JSON>>
```

Template configuration JSONs are given below for all the modules that are a part
of this sample.

### Linux

#### SQLite module configuration

```json    
{
    "name": "sqlite",
    "loader": {
        "name": "native",
        "entrypoint": {
            "module.path": "../../modules/modbus_read/libsqlite.so"
        }
    },
    "args": {
        "macAddress": "02:02:02:02:02:02",
        "sources": [
                {
            "id": "modbus",
            "dbPath": "./test.db",
            "table": "MODBUS",
            "limit": "10",
            "columns": [
                    {
                        "name": "DATETIME",
                        "type": "CHAR(19)",
                        "primaryKey": "1",
                        "notNull": "1"
                    },
                    {
                        "name": "MAC",
                        "type": "CHAR(17)",
                        "primaryKey": "1",
                        "notNull": "1"
                    },
                    {
                        "name": "VALUE",
                        "type": "INT",
                        "primaryKey": "0",
                        "notNull": "1"
                    },
                    {
                        "name": "ADDRESS",
                        "type": "INT",
                        "primaryKey": "1",
                        "notNull": "1"
                    }
                ]
            }
        ]
    }
}
```

#### Modbus module configuration

```json
{
    "name": "modbus_read",
    "loader": {
        "name": "native",
        "entrypoint": {
            "module.path": "../../modules/modbus_read/libmodbus_read.so"
        }
    },
    "args": [
        {
            "serverConnectionString": "COM1",
            "interval": "2000",
            "macAddress": "01:01:01:01:01:01",
            "deviceType": "powerMeter",
            "sqliteEnabled": "1",
            "operations": [
                {
                    "unitId": "1",
                    "functionCode": "3",
                    "startingAddress": "1",
                    "length": "5"
                }
            ]
        }
    ]
}
```

#### IoT Hub module

```json
{
    "name": "IoTHub",
    "loader": {
        "name": "native",
        "entrypoint": {
            "module.path": "../../modules/iothub/libiothub.so"
        }
    },
    "args": {
        "IoTHubName": "YOUR IOT HUB NAME",
        "IoTHubSuffix": "YOUR IOT HUB SUFFIX",
        "Transport": "TRANSPORT PROTOCOL"
    }
}
```

### Identity mapping module configuration

```json
{
    "name": "mapping",
    "loader": {
        "name": "native",
        "entrypoint": {
            "module.path": "../../modules/identitymap/libidentity_map.so"
        }
    },
    "args": [
        {
            "macAddress": "02:02:02:02:02:02",
            "deviceId": "YOUR DEVICE ID",
            "deviceKey": "YOUR DEVICE KEY"
        }
    ]
}
```

### Windows ##

#### SQLite module configuration

```json    
{
    "name": "sqlite",
    "loader": {
        "name": "native",
        "entrypoint": {
            "module.path": "..\\..\\..\\modules\\sqlite\\Debug\\sqlite.dll"
        }
    },
    "args": {
        "macAddress": "02:02:02:02:02:02",
        "sources": [
                {
            "id": "modbus",
            "dbPath": "D:\\test.db",
            "table": "MODBUS",
            "limit": "10",
            "columns": [
                    {
                        "name": "DATETIME",
                        "type": "CHAR(19)",
                        "primaryKey": "1",
                        "notNull": "1"
                    },
                    {
                        "name": "MAC",
                        "type": "CHAR(17)",
                        "primaryKey": "1",
                        "notNull": "1"
                    },
                    {
                        "name": "VALUE",
                        "type": "INT",
                        "primaryKey": "0",
                        "notNull": "1"
                    },
                    {
                        "name": "ADDRESS",
                        "type": "INT",
                        "primaryKey": "1",
                        "notNull": "1"
                    }
                ]
            }
        ]
    }
}
```

#### Modbus module configuration

```json
{
    "name": "modbus_read",
    "loader": {
        "name": "native",
        "entrypoint": {
            "module.path": "..\\..\\..\\modules\\modbus_read\\Debug\\modbus_read.dll"
        }
    },
    "args": [
        {
            "serverConnectionString": "127.0.0.1",
            "interval": "2000",
            "macAddress": "01:01:01:01:01:01",
            "deviceType": "powerMeter",
            "sqliteEnabled": "1",
            "operations": [
                {
                    "unitId": "1",
                    "functionCode": "3",
                    "startingAddress": "1",
                    "length": "1"
                }
            ]
        }
    ]
}
```

#### IoT Hub module

```json
{
    "name": "IoTHub",
    "loader": {
        "name": "native",
        "entrypoint": {
            "module.path": "..\\..\\..\\modules\\iothub\\Debug\\iothub.dll"
        }
    },
    "args": {
        "IoTHubName": "YOUR IOT HUB NAME",
        "IoTHubSuffix": "YOUR IOT HUB SUFFIX",
        "Transport": "TRANSPORT PROTOCOL"
    }
}
```

#### Identity mapping module configuration

```json
{
    "name": "mapping",
    "loader": {
        "name": "native",
        "entrypoint": {
            "module.path": "..\\..\\..\\modules\\identitymap\\Debug\\identity_map.dll"
        }
    },
    "args": [
        {
            "macAddress": "02:02:02:02:02:02",
            "deviceId": "YOUR DEVICE ID",
            "deviceKey": "YOUR DEVICE KEY"
        }
    ]
}
```

Links
-----
Links are to specify the message flow controlled by the message broker.
```json
  "links": [
    {
      "source": "mapping",
      "sink": "IoTHub"
    },
    {
      "source": "IoTHub",
      "sink": "mapping"
    },
    {
      "source": "mapping",
      "sink": "sqlite"
    },
    {
      "source": "sqlite",
      "sink": "mapping"
    },
    {
      "source": "modbus_read",
      "sink": "sqlite"
    }
  ]
```

## Sending cloud-to-device messages ##

You should be able to use the
[Azure IoT Hub Device Explorer](https://github.com/Azure/azure-iot-sdks/blob/master/tools/DeviceExplorer/doc/how_to_use_device_explorer.md) or the [IoT Hub Explorer](https://github.com/Azure/azure-iot-sdks/tree/master/tools/iothub-explorer)
to craft and send JSON messages that are handled and passed on to the SQLite databases
by the SQLite module. For example, sending the following JSON message to the device
via IoT Hub will query all data from the MODBUS table from D:\\test.db:

    ```json
    {
      "dbPath": "D:\\test.db",
      "sqlCommand": "select * from MODBUS;"
    }
    ```
## Receiving query result via device-to-cloud messages ##

In order to verify the result from previous section. Switch to the Data tab on Azure IoT Hub Device Explorer.
Start monitoring the SQLite device. Resend the cloud-to-device command and the result should be received.