namespace SQLite
{
    using System;
    using System.Collections.Generic;
    using System.IO;
    using System.Runtime.InteropServices;
    using System.Runtime.Loader;
    using System.Security.Cryptography.X509Certificates;
    using System.Text;
    using System.Threading;
    using System.Threading.Tasks;
    using Microsoft.Azure.Devices.Client;
    using Microsoft.Azure.Devices.Client.Transport.Mqtt;
    using Microsoft.Azure.Devices.Shared;
    using Newtonsoft.Json;
    using Microsoft.Data.Sqlite;

    class Program
    {
        const string SQLiteConfigs = "SQLiteConfigs";
        const int DefaultPushInterval = 5000;
        static int m_counter = 0;

        static void Main(string[] args)
        {
            // Initialize Edge Module
            InitEdgeModule().Wait();

            // Wait until the app unloads or is cancelled
            var cts = new CancellationTokenSource();
            AssemblyLoadContext.Default.Unloading += (ctx) => cts.Cancel();
            Console.CancelKeyPress += (sender, cpe) => cts.Cancel();
            WhenCancelled(cts.Token).Wait();
        }

        /// <summary>
        /// Handles cleanup operations when app is cancelled or unloads
        /// </summary>
        public static Task WhenCancelled(CancellationToken cancellationToken)
        {
            var tcs = new TaskCompletionSource<bool>();
            cancellationToken.Register(s => ((TaskCompletionSource<bool>)s).SetResult(true), tcs);
            return tcs.Task;
        }

        /// <summary>
        /// Initializes the Azure IoT Client for the Edge Module
        /// </summary>
        static async Task InitEdgeModule()
        {
            try
            {
                // Open a connection to the Edge runtime using MQTT transport and
                // the connection string provided as an environment variable
                string connectionString = Environment.GetEnvironmentVariable("EdgeHubConnectionString");
               
                AmqpTransportSettings amqpSettings = new AmqpTransportSettings(TransportType.Amqp_Tcp_Only);
                // Suppress cert validation on Windows for now
                /*
                if (RuntimeInformation.IsOSPlatform(OSPlatform.Windows))
                {
                    amqpSettings.RemoteCertificateValidationCallback = (sender, certificate, chain, sslPolicyErrors) => true;
                }
                */


              
                ITransportSettings[] settings = { amqpSettings };

                ModuleClient ioTHubModuleClient = await ModuleClient.CreateFromEnvironmentAsync(settings);
                await ioTHubModuleClient.OpenAsync();
                Console.WriteLine("IoT Hub module client initialized.");

                // Read config from Twin and Start
                Twin moduleTwin = await ioTHubModuleClient.GetTwinAsync();
                await UpdateStartFromTwin(moduleTwin.Properties.Desired, ioTHubModuleClient);

                // Attach callback for Twin desired properties updates
                await ioTHubModuleClient.SetDesiredPropertyUpdateCallbackAsync(OnDesiredPropertiesUpdate, ioTHubModuleClient);

            }
            catch (AggregateException ex)
            {
                foreach (Exception exception in ex.InnerExceptions)
                {
                    Console.WriteLine();
                    Console.WriteLine("Error when initializing module: {0}", exception);
                }
            }

        }

        /// <summary>
        /// This method is called whenever the module is sent a message from the EdgeHub. 
        /// It just pipe the messages without any change.
        /// It prints all the incoming messages.
        /// </summary>
        static async Task<MessageResponse> PipeMessage(Message message, object userContext)
        {
            Console.WriteLine("SQLite - Received command");
            int counterValue = Interlocked.Increment(ref m_counter);

            var userContextValues = userContext as Tuple<ModuleClient, ModuleHandle>;
            if (userContextValues == null)
            {
                throw new InvalidOperationException("UserContext doesn't contain " +
                    "expected values");
            }
            ModuleClient ioTHubModuleClient = userContextValues.Item1;
            ModuleHandle moduleHandle = userContextValues.Item2;

            byte[] messageBytes = message.GetBytes();
            string messageString = Encoding.UTF8.GetString(messageBytes);
            Console.WriteLine($"Received message: {counterValue}, Body: [{messageString}]");

            message.Properties.TryGetValue("command-type", out string cmdType);
            if (cmdType == "SQLiteCmd")
            {
                // Get message body, containing the write target and value
                var messageBody = JsonConvert.DeserializeObject<SQLiteInMessage>(messageString);

                if (messageBody != null)
                {
                    using (var transaction = moduleHandle.connection.BeginTransaction())
                    {
                        var selectCommand = moduleHandle.connection.CreateCommand();
                        selectCommand.Transaction = transaction;
                        //selectCommand.CommandText = "SELECT * FROM test;";
                        selectCommand.CommandText = messageBody.Command;
                        using (var reader = selectCommand.ExecuteReader())
                        {
                            if(reader.HasRows)
                            {
                                SQLiteOutMessage out_message = new SQLiteOutMessage();
                                out_message.PublishTimestamp = DateTime.Now.ToString("yyyy-MM-dd HH:mm:ss");

                                while (reader.Read())
                                {
                                    List<string> row = new List<string>();
                                    int count = reader.FieldCount;
                                    for(int i = 0; i<count; i++)
                                    {
                                        var value = reader.GetString(i);
                                        row.Add(value);
                                    }
                                    out_message.Rows.Add(row);
                                }

                                //todo send back to sender module
                                message = new Message(Encoding.ASCII.GetBytes(JsonConvert.SerializeObject(out_message)));
                                message.Properties.Add("content-type", "application/edge-sqlite-json");

                                if (message != null)
                                {
                                    await ioTHubModuleClient.SendEventAsync("sqliteOutput", message);
                                }
                            }
                        }
                        transaction.Commit();
                    }
                }
            }
            return MessageResponse.Completed;
        }

        /// <summary>
        /// Callback to handle Twin desired properties updates�
        /// </summary>
        static async Task OnDesiredPropertiesUpdate(TwinCollection desiredProperties, object userContext)
        {
            ModuleClient ioTHubModuleClient = userContext as ModuleClient;

            try
            {
                // stop all activities while updating configuration
                await ioTHubModuleClient.SetInputMessageHandlerAsync(
                "input1",
                DummyCallBack,
                null);

                await UpdateStartFromTwin(desiredProperties, ioTHubModuleClient);
            }
            catch (AggregateException ex)
            {
                foreach (Exception exception in ex.InnerExceptions)
                {
                    Console.WriteLine();
                    Console.WriteLine("Error when receiving desired property: {0}", exception);
                }
            }
            catch (Exception ex)
            {
                Console.WriteLine();
                Console.WriteLine("Error when receiving desired property: {0}", ex.Message);
            }
        }

        /// <summary>
        /// A dummy callback does nothing
        /// </summary>
        /// <param name="message"></param>
        /// <param name="userContext"></param>
        /// <returns></returns>
        static async Task<MessageResponse> DummyCallBack(Message message, object userContext)
        {
            await Task.Delay(TimeSpan.FromSeconds(0));
            return MessageResponse.Abandoned;
        }

        /// <summary>
        /// Update Start from module Twin. 
        /// </summary>
        static async Task UpdateStartFromTwin(TwinCollection desiredProperties, ModuleClient ioTHubModuleClient)
        {
            ModuleConfig config;
            ModuleHandle moduleHandle;
            string jsonStr = null;
            string serializedStr;

            serializedStr = JsonConvert.SerializeObject(desiredProperties);
            Console.WriteLine("Desired property change:");
            Console.WriteLine(serializedStr);

            if (desiredProperties.Contains(SQLiteConfigs))
            {
                // get config from Twin
                jsonStr = serializedStr;
            }
            else
            {
                Console.WriteLine("No configuration found in desired properties, look in local...");
                if (File.Exists(@"iot-edge-sqlite.json"))
                {
                    try
                    {
                        // get config from local file
                        jsonStr = File.ReadAllText(@"iot-edge-sqlite.json");
                    }
                    catch (Exception ex)
                    {
                        Console.WriteLine("Load configuration error: " + ex.Message);
                    }
                }
                else
                {
                    Console.WriteLine("No configuration found in local file.");
                }
            }

            if (!string.IsNullOrEmpty(jsonStr))
            {
                Console.WriteLine("Attempt to load configuration: " + jsonStr);
                config = JsonConvert.DeserializeObject<ModuleConfig>(jsonStr);

                if (config.IsValidate())
                {
                    moduleHandle = ModuleHandle.CreateHandleFromConfiguration(config);

                    if (moduleHandle != null)
                    {
                        var userContext = new Tuple<ModuleClient, ModuleHandle>(ioTHubModuleClient, moduleHandle);
                        // Register callback to be called when a message is received by the module
                        await ioTHubModuleClient.SetInputMessageHandlerAsync(
                        "input1",
                        PipeMessage,
                        userContext);
                    }
                }
            }
        }

        
    }
    class ModuleHandle
    {
        public SqliteConnection connection;
        public static ModuleHandle CreateHandleFromConfiguration(ModuleConfig config)
        {
            ModuleHandle handle = new ModuleHandle();
            handle.connection = new SqliteConnection("" +
            new SqliteConnectionStringBuilder
            {
                DataSource = config.DbPath
            });
            try
            {
                handle.connection.Open();

                using(var command = handle.connection.CreateCommand())  
                {  
                    string columns = "";
                    string primaryKey = "PRIMARY KEY(";
                    foreach(var column in config.Columns)
                    {
                        columns+=$"{column.Value.Name} {column.Value.Type} {(column.Value.NotNull?"NOT NULL":"")},";
                        if(column.Value.IsKey)
                            primaryKey+=$"{column.Value.Name},";
                    }
                    columns.Remove(columns.Length - 1);
                    primaryKey = primaryKey.Remove(primaryKey.Length - 1) + ')';
                    
                    command.CommandText = $@"  
                    CREATE TABLE IF NOT EXISTS {config.TableName}  
                    (  
                        {columns} {primaryKey}
                    );
                    ";  
            
                    // Create table if not exist  
                    command.ExecuteNonQuery();  

                    //test
                    /*
                    using (var transaction = handle.connection.BeginTransaction())
                    {
                        var insertCommand = handle.connection.CreateCommand();
                        insertCommand.Transaction = transaction;
                        insertCommand.CommandText = "INSERT INTO test ( Id, Value ) VALUES ( 2,99 )";
                        
                        insertCommand.ExecuteNonQuery();
                        

                        var selectCommand = handle.connection.CreateCommand();
                        selectCommand.Transaction = transaction;
                        selectCommand.CommandText = "SELECT * FROM test";
                        using (var reader = selectCommand.ExecuteReader())
                        {
                            while (reader.Read())
                            {
                                int count = reader.FieldCount;
                                string message = "";
                                for(int i = 0; i<count; i++)
                                {
                                    message += reader.GetString(i)+' ';
                                }
                                Console.WriteLine(message);
                            }
                        }
                        transaction.Commit();
                    }
                    */
                }
            }
            catch(Exception e)
            {
                Console.WriteLine($"Exception while opening database, err message: {e.Message}");
                Console.WriteLine("Check if the database file is created or being mounted into the conainter correctly");
            }

            return handle;
        }
    }
    
    class SQLiteOutMessage
    {
        public string PublishTimestamp;
        public List<List<string>> Rows = new List<List<string>>();
    }
    class SQLiteInMessage
    {
        public string Command;
    }
    class ModuleConfig
    {
        public string DbPath;
        public string TableName;
        public Dictionary<string, Column> Columns;
        public bool IsValidate()
        {
            bool ret = true;
            if(DbPath == null)
            {
                Console.WriteLine("missing DbPath");
                ret = false;
            }
            if(TableName == null)
            {
                Console.WriteLine("missing TableName");
            }
            
            if(Columns.Count == 0)
            {
                Console.WriteLine("missing Columns");
            }
            else
            {
                foreach(var column in Columns)
                {
                    if(column.Value.Name == null)
                    {
                        Console.WriteLine($"Column:{column.Key} missing Name");
                    }
                    if(column.Value.Type == null)
                    {
                        Console.WriteLine($"Column:{column.Key} missing Type");
                    }
                }
            }
            return ret;
        }
    }
    class Column
    {
        public string Name;
        public string Type;
        public bool IsKey;
        public bool NotNull;
    }
}