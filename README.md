This project has adopted the [Microsoft Open Source Code of Conduct](https://opensource.microsoft.com/codeofconduct/). For more information see the [Code of Conduct FAQ](https://opensource.microsoft.com/codeofconduct/faq/) or contact [opencode@microsoft.com](mailto:opencode@microsoft.com) with any additional questions or comments

# Azure IoT Edge SQLite Module GA #
Using this module, developers can build Azure IoT Edge solutions with capability to access SQLite databases. The SQLite module is an [Azure IoT Edge](https://github.com/Azure/iot-edge) module, capable of executing sql queries sent from other modules, and return result to the senders or to the Azure IoT Hub via the Edge framework. Developers can modify the module tailoring to any scenario.

![](./doc/diagram.png)

There are prebuilt SQLite module container images ready at [here](https://hub.docker.com/r/microsoft/azureiotedge-sqlite) for you to quickstart the experience of Azure IoT Edge on your target device or simulated device.

Visit http://azure.com/iotdev to learn more about developing applications for Azure IoT.

## Azure IoT Edge Compatibility ##
Current version of the module is targeted for the [Azure IoT Edge GA](https://azure.microsoft.com/en-us/blog/azure-iot-edge-generally-available-for-enterprise-grade-scaled-deployments/).  
If you are using [v1 version of IoT Edge](https://github.com/Azure/iot-edge/tree/master/v1) (previously known as Azure IoT Gateway), please use v1 version of this module, all materials can be found in [v1](https://github.com/Azure/iot-edge-sqlite/tree/master/v1) folder.

Find more information about Azure IoT Edge at [here](https://docs.microsoft.com/en-us/azure/iot-edge/how-iot-edge-works).

## Target Device Setup ##

### Platform Compatibility ###
Azure IoT Edge is designed to be used with a broad range of operating system platforms. SQLite module has been tested on the following platforms:

- Windows 10 Enterprise (version 1709) x64
- Windows 10 IoT Core (version 1709) x64
- Linux x64
- Linux arm32v7

### Device Setup ###
- [Windows 10 Desktop](https://docs.microsoft.com/en-us/azure/iot-edge/quickstart)
- [Windows 10 IoT Core](https://docs.microsoft.com/en-us/azure/iot-edge/how-to-install-iot-core)
- [Linux](https://docs.microsoft.com/en-us/azure/iot-edge/quickstart-linux)


## Build Environment Setup ##
SQLite module is a .NET Core 2.1 application, which is developed and built based on the guidelines in Azure IoT Edge document.
Please follow [this link](https://docs.microsoft.com/en-us/azure/iot-edge/tutorial-csharp-module) to setup the build environment. 

Basic requirement:
- Docker CE
- .NET Core 2.1 SDK

## HowTo Build ##
In this section, the SQLite module we be built as an IoT Edge module.

Open the project in VS Code, and open VS Code command palette, type and run the command Edge: Build IoT Edge solution.
Select the deployment.template.json file for your solution from the command palette.  
***Note: Be sure to check [configuration section](https://github.com/Azure/iot-edge-sqlite#configuration) to properly set each fields before deploying the module.*** 

In Azure IoT Hub Devices explorer, right-click an IoT Edge device ID, then select Create deployment for IoT Edge device. 
Open the config folder of your solution, then select the deployment.json file. Click Select Edge Deployment Manifest. 
Then you can see the deployment is successfully created with a deployment ID in VS Code integrated terminal.
You can check your container status in the VS Code Docker explorer or by run the docker ps command in the terminal.

## Configuration ##
Before running the module, proper configuration is required. Here is a sample of the desired properties for your reference.
```json
{
  "SQLiteConfigs":{
    "Db01":{
      "DbPath": "/app/db/test.db",
      "Table01":{
        "TableName": "test",
        "Column01":{
          "ColumnName": "Id",
          "Type": "numeric",
          "IsKey": "true",
          "NotNull": "true"
        },
        "Column02":{
          "ColumnName": "Value",
          "Type": "numeric",
          "IsKey": "false",
          "NotNull": "true"
        }
      }
    }
  }
}
```
Meaning of each field:

* "SQLiteConfigs" - Contains one or more SQLite databases' configuration. In this sample, we have "Db01":
    * "Db01" - User defined names for each SQLite database, cannot have duplicates under "SQLiteConfigs".
      * "DbPath" - The absolute path to the db file.
      * "Table01" - User defined names for each table, cannot have duplicates under the same db.
      * "TableName" - Table name of Table01.
          * "Column01", "Column02" - User defined names for each column, cannot have duplicates under the same table.
            * "ColumnName": Column name of Column01, Column02.
            * "Type" - Data type of the column
            * "IsKey" - Is key property of the column
            * "NotNull" - Is notnull property of the column

## Module Endpoints and Routing ##
There are two endpoints defined in SQLite module:  
- "sqliteOutput": This is an output endpoint for the result of sql queries.
- "input1": This is an input endpoint for sql queries.

Input/Output message format and Routing rules are introduced below.

### Send SQL Queries to SQLite ###
SQLite module use input endpoint "input1" to receive commands. 
***Note: Currently IoT Edge only supports send messages into one module from another module, direct C2D messages doesn't work.*** 

#### Command Message ####
The content of command must be the following message format.  

Message Properties: 
```json
"command-type": "SQLiteCmd"
```

Message Payload:
```json
{
    "RequestId":"0",
    "RequestModule":"filter",
    "DbName":"/app/db/test.db",
    "Command":"select Id, Value from test;"
}
```

#### Route from other (filter) modules ####
The command should have a property "command-type" with value "SQLiteCmd". Also, routing must be enabled by specifying rule like below.
```json
{
  "routes": {
    "filterToSQLite":"FROM /messages/modules/filtermodule/outputs/output1 INTO BrokeredEndpoint(\"/modules/sqlite/inputs/input1\")"
  }
}
```
### Receive Result from SQLite ###

#### Result Message ####
Message Properties: 
```json
"content-type": "application/edge-sqlite-json"
```
Message Payload:
```json
[
  {
    "PublishTimestamp":"2018-09-04 04:16:47",
    "RequestId":"0",
    "RequestModule":"filter",
    "Rows":[
      [
        "1",
        "20"
      ],
      [
        "2",
        "100"
      ]
    ]
  }
]
```

#### Route to IoT Hub ####
```json
{
  "routes": {
    "sqliteToIoTHub":"FROM /messages/modules/sqlite/outputs/sqliteOutput INTO $upstream"
  }
}
```

#### Route to other (filter) modules ####
```json
{
  "routes": {
    "sqliteToFilter":"FROM /messages/modules/sqlite/outputs/sqliteOutput INTO BrokeredEndpoint(\"/modules/filtermodule/inputs/input1\")"
  }
}
```

## HowTo Run ##

### Run as an IoT Edge module ###
Please follow the [link](https://docs.microsoft.com/en-us/azure/iot-edge/tutorial-csharp-module) to deploy the module as an IoT Edge module.
