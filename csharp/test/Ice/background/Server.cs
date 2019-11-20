//
// Copyright (c) ZeroC, Inc. All rights reserved.
//

using Test;
using Ice;
using System.Reflection;
using System.Threading.Tasks;

[assembly: AssemblyTitle("IceTest")]
[assembly: AssemblyDescription("Ice test")]
[assembly: AssemblyCompany("ZeroC, Inc.")]

public class Server : TestHelper
{
    internal class LocatorI : Ice.Locator
    {
        public Task<Ice.ObjectPrx>
        findAdapterByIdAsync(string adapter, Ice.Current current)
        {
            _controller.checkCallPause(current);
            Ice.Communicator communicator = current.adapter.getCommunicator();
            return Task<Ice.ObjectPrx>.FromResult(current.adapter.createDirectProxy(Ice.Util.stringToIdentity("dummy")));
        }

        public Task<Ice.ObjectPrx>
        findObjectByIdAsync(Ice.Identity id, Ice.Current current)
        {
            _controller.checkCallPause(current);
            return Task<Ice.ObjectPrx>.FromResult(current.adapter.createDirectProxy(id));
        }

        public Ice.LocatorRegistryPrx getRegistry(Ice.Current current)
        {
            return null;
        }

        internal LocatorI(BackgroundControllerI controller)
        {
            _controller = controller;
        }

        private BackgroundControllerI _controller;
    }

    internal class RouterI : Ice.Router
    {
        public Ice.ObjectPrx getClientProxy(out Ice.Optional<bool> hasRoutingTable, Ice.Current current)
        {
            hasRoutingTable = new Ice.Optional<bool>(true);
            _controller.checkCallPause(current);
            return null;
        }

        public Ice.ObjectPrx getServerProxy(Ice.Current current)
        {
            _controller.checkCallPause(current);
            return null;
        }

        public Ice.ObjectPrx[] addProxies(Ice.ObjectPrx[] proxies, Ice.Current current)
        {
            return new Ice.ObjectPrx[0];
        }

        internal RouterI(BackgroundControllerI controller)
        {
            _controller = controller;
        }

        private BackgroundControllerI _controller;
    }

    public override void run(string[] args)
    {
        var properties = createTestProperties(ref args);
        //
        // This test kills connections, so we don't want warnings.
        //
        properties.setProperty("Ice.Warn.Connections", "0");

        properties.setProperty("Ice.MessageSizeMax", "50000");

        // This test relies on filling the TCP send/recv buffer, so
        // we rely on a fixed value for these buffers.
        properties.setProperty("Ice.TCP.RcvSize", "50000");

        //
        // Setup the test transport plug-in.
        //
        properties.setProperty("Ice.Default.Protocol",
                               "test-" + properties.getPropertyWithDefault("Ice.Default.Protocol", "tcp"));

        using (var communicator = initialize(properties))
        {
            PluginI plugin = new PluginI(communicator);
            plugin.initialize();
            communicator.getPluginManager().addPlugin("Test", plugin);

            //
            // When running as a MIDlet the properties for the server may be
            // overridden by configuration. If it isn't then we assume
            // defaults.
            //
            if (communicator.getProperties().getProperty("TestAdapter.Endpoints").Length == 0)
            {
                communicator.getProperties().setProperty("TestAdapter.Endpoints", getTestEndpoint(0));
            }

            if (communicator.getProperties().getProperty("ControllerAdapter.Endpoints").Length == 0)
            {
                communicator.getProperties().setProperty("ControllerAdapter.Endpoints", getTestEndpoint(1, "tcp"));
                communicator.getProperties().setProperty("ControllerAdapter.ThreadPool.Size", "1");
            }

            Ice.ObjectAdapter adapter = communicator.createObjectAdapter("TestAdapter");
            Ice.ObjectAdapter adapter2 = communicator.createObjectAdapter("ControllerAdapter");

            BackgroundControllerI backgroundController = new BackgroundControllerI(adapter);

            BackgroundI backgroundI = new BackgroundI(backgroundController);

            adapter.Add(backgroundI, Ice.Util.stringToIdentity("background"));

            LocatorI locatorI = new LocatorI(backgroundController);
            adapter.Add(locatorI, Ice.Util.stringToIdentity("locator"));

            RouterI routerI = new RouterI(backgroundController);
            adapter.Add(routerI, Util.stringToIdentity("router"));
            adapter.activate();

            adapter2.Add(backgroundController, Util.stringToIdentity("backgroundController"));
            adapter2.activate();

            communicator.waitForShutdown();
        }
    }

    public static int Main(string[] args)
    {
        return Test.TestDriver.runTest<Server>(args);
    }
}
