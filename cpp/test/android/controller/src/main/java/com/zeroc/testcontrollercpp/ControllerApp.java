//
// Copyright (c) ZeroC, Inc. All rights reserved.
//

package com.zeroc.testcontrollercpp;

import java.io.*;
import java.util.*;

import com.zeroc.Ice.Logger;
import com.zeroc.Ice.Communicator;
import com.zeroc.IceInternal.Time;

import android.os.Build;
import android.util.Log;
import android.app.Application;

import Test.Common.ProcessControllerRegistryPrx;
import Test.Common.ProcessControllerPrx;

public class ControllerApp extends Application
{
    private final String TAG = "ControllerApp";
    private ControllerI _controllerI;
    private ControllerActivity _activity;
    private String _ipv4Address;
    private String _ipv6Address;

    class AndroidLogger implements Logger
    {
        private final String _prefix;

        AndroidLogger(String prefix)
        {
            _prefix = prefix;
        }

        @Override
        public void print(String message)
        {
            Log.d(TAG, message);
        }

        @Override
        public void trace(String category, String message)
        {
            Log.v(category, message);
        }

        @Override
        public void warning(String message)
        {
            Log.w(TAG, message);
        }

        @Override
        public void error(String message)
        {
            Log.e(TAG, message);
        }

        @Override
        public String getPrefix()
        {
            return _prefix;
        }

        @Override
        public Logger cloneWithPrefix(String s)
        {
            return new AndroidLogger(s);
        }
    }

    @Override
    public void onCreate()
    {
        super.onCreate();
        com.zeroc.Ice.Util.setProcessLogger(new AndroidLogger(""));
    }

    synchronized public void setIpv4Address(String address)
    {
        _ipv4Address = address;
    }

    synchronized public void setIpv6Address(String address)
    {
        int i = address.indexOf("%");
        _ipv6Address = i == -1 ? address : address.substring(i);
    }

    public List<String> getAddresses(boolean ipv6)
    {
        List<String> addresses = new java.util.ArrayList<String>();
        try
        {
            java.util.Enumeration<java.net.NetworkInterface> ifaces = java.net.NetworkInterface.getNetworkInterfaces();
            while(ifaces.hasMoreElements())
            {
                java.net.NetworkInterface iface = ifaces.nextElement();
                java.util.Enumeration<java.net.InetAddress> addrs = iface.getInetAddresses();
                while(addrs.hasMoreElements())
                {
                    java.net.InetAddress addr = addrs.nextElement();
                    if((ipv6 && addr instanceof java.net.Inet6Address) ||
                       (!ipv6 && !(addr instanceof java.net.Inet6Address)))
                    {
                        addresses.add(addr.getHostAddress());
                    }
                }
            }
        }
        catch(java.net.SocketException ex)
        {
        }
        return addresses;
    }

    public synchronized void startController(ControllerActivity activity)
    {
        _activity = activity;
        if(_controllerI == null)
        {
            _controllerI = new ControllerI();
        }
    }

    public synchronized void println(final String data)
    {
        _activity.runOnUiThread(new Runnable()
                                {
                                    @Override
                                    public void run()
                                    {
                                        synchronized(ControllerApp.this)
                                        {
                                            _activity.println(data);
                                        }
                                    }
                                });
    }

    public static boolean isEmulator()
    {
        return Build.FINGERPRINT.startsWith("generic") ||
               Build.FINGERPRINT.startsWith("unknown") ||
               Build.MODEL.contains("google_sdk") ||
               Build.MODEL.contains("Emulator") ||
               Build.MODEL.contains("Android SDK built for x86") ||
               Build.MANUFACTURER.contains("Genymotion") ||
               (Build.BRAND.startsWith("generic") && Build.DEVICE.startsWith("generic")) ||
               Build.PRODUCT.equals("google_sdk");
    }

    class ControllerI
    {
        public ControllerI()
        {
            com.zeroc.Ice.InitializationData initData = new com.zeroc.Ice.InitializationData();
            initData.properties = com.zeroc.Ice.Util.createProperties();
            initData.properties.setProperty("Ice.ThreadPool.Server.SizeMax", "10");
            initData.properties.setProperty("ControllerAdapter.Endpoints", "tcp");
            //initData.properties.setProperty("Ice.Trace.Network", "3");
            //initData.properties.setProperty("Ice.Trace.Protocol", "1");
            initData.properties.setProperty("ControllerAdapter.AdapterId", java.util.UUID.randomUUID().toString());
            initData.properties.setProperty("Ice.Override.ConnectTimeout", "1000");
            if(!isEmulator())
            {
                initData.properties.setProperty("Ice.Plugin.IceDiscovery", "com.zeroc.IceDiscovery.PluginFactory");
                initData.properties.setProperty("IceDiscovery.DomainId", "TestController");
            }
            _communicator = com.zeroc.Ice.Util.initialize(initData);
            com.zeroc.Ice.ObjectAdapter adapter = _communicator.createObjectAdapter("ControllerAdapter");
            ProcessControllerPrx processController = ProcessControllerPrx.uncheckedCast(
                    adapter.add(new ProcessControllerI(),
                                com.zeroc.Ice.Util.stringToIdentity("Android/ProcessController")));
            adapter.activate();
            ProcessControllerRegistryPrx registry;
            if(isEmulator())
            {
                registry = ProcessControllerRegistryPrx.uncheckedCast(
                        _communicator.stringToProxy("Util/ProcessControllerRegistry:tcp -h 10.0.2.2 -p 15001"));
            }
            else
            {
                // Use IceDiscovery to find a process controller registry
                registry = ProcessControllerRegistryPrx.uncheckedCast(
                        _communicator.stringToProxy("Util/ProcessControllerRegistry"));
            }
            registerProcessController(adapter, registry, processController);
            println("Android/ProcessController");
        }

        public void
        registerProcessController(final com.zeroc.Ice.ObjectAdapter adapter,
                                  final ProcessControllerRegistryPrx registry,
                                  final ProcessControllerPrx processController)
        {
            registry.ice_pingAsync().whenCompleteAsync(
                (r1, e1) ->
                {
                    if(e1 != null)
                    {
                        handleException(e1, adapter, registry, processController);
                    }
                    else
                    {
                        com.zeroc.Ice.Connection connection = registry.ice_getConnection();
                        connection.setAdapter(adapter);
                        connection.setACM(OptionalInt.of(5),
                                Optional.of(com.zeroc.Ice.ACMClose.CloseOff),
                                Optional.of(com.zeroc.Ice.ACMHeartbeat.HeartbeatAlways));
                        connection.setCloseCallback(
                                con ->
                                {
                                    println("connection with process controller registry closed");
                                    while (true) {
                                        try
                                        {
                                            Thread.sleep(500);
                                            break;
                                        }
                                        catch(InterruptedException e)
                                        {
                                        }
                                    }
                                    registerProcessController(adapter, registry, processController);
                                });

                        registry.setProcessControllerAsync(processController).whenCompleteAsync(
                                (r2, e2) ->
                                {
                                    if(e2 != null)
                                    {
                                        handleException(e2, adapter, registry, processController);
                                    }
                                });
                    }
                });
        }

        public void handleException(Throwable ex,
                                    final com.zeroc.Ice.ObjectAdapter adapter,
                                    final ProcessControllerRegistryPrx registry,
                                    final ProcessControllerPrx processController)
        {
            if(ex instanceof com.zeroc.Ice.ConnectFailedException || ex instanceof com.zeroc.Ice.TimeoutException)
            {
                while(true)
                {
                    try
                    {
                        Thread.sleep(500);
                        break;
                    }
                    catch(InterruptedException e)
                    {
                    }
                }
                registerProcessController(adapter, registry, processController);
            }
            else
            {
                println(ex.toString());
            }
        }

        public void destroy()
        {
            _communicator.destroy();
        }

        private ProcessControllerRegistryPrx _registry;
        private com.zeroc.Ice.Communicator _communicator;
    }

    class ControllerHelperI
    {
        public ControllerHelperI(String name, String suffix, String exe, String[] args)
        {
            _helper = createControllerHelper(name, suffix, exe, args);
        }

        public void shutdown()
        {
            shutdown(_helper);
        }

        public String getOutput()
        {
            return getOutput(_helper);
        }

        private void waitReady(int timeout)
            throws Test.Common.ProcessFailedException
        {
            int status = waitReady(_helper, timeout);
            if(status < 0)
            {
                throw new Test.Common.ProcessFailedException("timed out waiting for the process to be ready");
            }
            else if(status > 0)
            {
                throw new Test.Common.ProcessFailedException("failure: " + getOutput());
            }
        }

        synchronized private int waitSuccess(int timeout)
            throws Test.Common.ProcessFailedException
        {
            int status = waitSuccess(_helper, timeout);
            if(status < 0)
            {
                throw new Test.Common.ProcessFailedException("timed out waiting for the process to be ready");
            }
            return status;
        }

        public native long createControllerHelper(String testsuite, String suffix, String exe, String[] args);
        public native void shutdown(long helper);
        public native String getOutput(long helper);
        public native int waitReady(long helper, int timeout);
        public native int waitSuccess(long helper, int timeout);

        private long _helper;
    }

    class ProcessControllerI implements Test.Common.ProcessController
    {
        public Test.Common.ProcessPrx start(final String testsuite, final String exe, String[] args,
                                            com.zeroc.Ice.Current current)
            throws Test.Common.ProcessFailedException
        {
            println("starting " + testsuite + " " + exe + "... ");

            String suffix = null;
            if(exe.equals("server"))
            {
                suffix = "sync";
            }
            else if(exe.equals("serveramd"))
            {
                suffix = "async";
            }
            else if(exe.equals("collocated"))
            {
                suffix = "collocated";
            }
            else if(exe.equals("client"))
            {
                if(_suffix != null)
                {
                    suffix = _suffix; // Use suffix from server exe
                    _suffix = null;
                }
                else
                {
                    suffix = "sync";
                }
            }

            if(exe.startsWith("server"))
            {
                _suffix = suffix; // Save the suffix for the client
            }

            try
            {
                ControllerHelperI mainHelper = new ControllerHelperI(testsuite, suffix, exe, args);
                return Test.Common.ProcessPrx.uncheckedCast(current.adapter.addWithUUID(new ProcessI(mainHelper)));
            }
            catch(Exception ex)
            {
                throw new Test.Common.ProcessFailedException(
                    "testsuite `" + testsuite + "' exe ` " + exe + "' start failed:\n" + ex.toString());
            }
        }

        public String getHost(String protocol, boolean ipv6, com.zeroc.Ice.Current current)
        {
            if(isEmulator())
            {
                return  "127.0.0.1";
            }
            else
            {
                synchronized(ControllerApp.this)
                {
                    return ipv6 ? _ipv6Address : _ipv4Address;
                }
            }
        }

        public String _suffix;
    }

    class ProcessI implements Test.Common.Process
    {
        public ProcessI(ControllerHelperI controllerHelper)
        {
            _controllerHelper = controllerHelper;
        }

        public void waitReady(int timeout, com.zeroc.Ice.Current current)
            throws Test.Common.ProcessFailedException
        {
            _controllerHelper.waitReady(timeout);
        }

        public int waitSuccess(int timeout, com.zeroc.Ice.Current current)
            throws Test.Common.ProcessFailedException
        {
            return _controllerHelper.waitSuccess(timeout);
        }

        public String terminate(com.zeroc.Ice.Current current)
        {
            _controllerHelper.shutdown();
            current.adapter.remove(current.id);
            return _controllerHelper.getOutput();
        }

        private ControllerHelperI _controllerHelper;
    }

    static
    {
        System.loadLibrary("controller");
    }
}
