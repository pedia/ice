//
// Copyright (c) ZeroC, Inc. All rights reserved.
//

using Test;
using Ice.servantLocator.Test;

namespace Ice
{
    namespace servantLocator
    {
        public class Server : TestHelper
        {
            public override void run(string[] args)
            {
                var initData = new InitializationData();
                initData.typeIdNamespaces = new string[] { "Ice.servantLocator.TypeId" };
                initData.properties = createTestProperties(ref args);
                using (var communicator = initialize(initData))
                {
                    communicator.getProperties().setProperty("TestAdapter.Endpoints", getTestEndpoint(0));
                    communicator.getProperties().setProperty("Ice.Warn.Dispatch", "0");

                    var adapter = communicator.createObjectAdapter("TestAdapter");
                    adapter.addServantLocator(new ServantLocatorI("category"), "category");
                    adapter.addServantLocator(new ServantLocatorI(""), "");
                    adapter.Add(new TestI(), Ice.Util.stringToIdentity("asm"));
                    adapter.Add(new TestActivationI(), Ice.Util.stringToIdentity("test/activation"));
                    adapter.activate();
                    serverReady();
                    adapter.waitForDeactivate();
                }
            }

            public static int Main(string[] args)
            {
                return TestDriver.runTest<Server>(args);
            }
        }
    }
}
