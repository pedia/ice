//
// Copyright (c) ZeroC, Inc. All rights reserved.
//

#include <jni.h>
#include <string>
#include <dlfcn.h>
#include <TestHelper.h>

#include <thread>
#include <sstream>

using namespace std;

namespace
{
    string toCpp(JNIEnv* env, jstring value)
    {
        auto cstr = env->GetStringUTFChars(value, 0);
        string s(cstr);
        env->ReleaseStringUTFChars(value, cstr);
        return s;
    }

    jstring toJava(JNIEnv* env, const string& value)
    {
        return env->NewStringUTF(value.c_str());
    }

    template<typename T, typename JT> vector<T> toVector(JNIEnv* env, jobjectArray array)
    {
        vector<T> vec;
        for(int i = 0; i < env->GetArrayLength(array); ++i)
        {
            JT v = static_cast<JT>(env->GetObjectArrayElement(array, i));
            vec.push_back(toCpp(env, v));
            env->DeleteLocalRef(v);
        }
        return vec;
    }

    typedef Test::TestHelper* (*CREATE_HELPER_ENTRY_POINT)();
    Test::StreamHelper streamRedirect;

    class ControllerHelperI : Test::ControllerHelper
    {
    public:

        ControllerHelperI(JNIEnv* env, jstring testsuite, jstring suffix, jstring exe, jobjectArray args) :
            _testsuite(toCpp(env, testsuite)),
            _suffix(toCpp(env, suffix)),
            _exe(toCpp(env, exe)),
            _args(toVector<string, jstring>(env, args)),
            _ready(false),
            _completed(false),
            _status(0)
        {
        }

        //
        // Called by the C++ TestHelper
        //
        virtual string loggerPrefix() const
        {
            return _testsuite + "/" + _exe;
        }

        virtual void print(const string& msg)
        {
            lock_guard<mutex> lock(_mutex);
            _out << msg;
        }

        virtual void serverReady()
        {
            lock_guard<mutex> lock(_mutex);
            _ready = true;
            _cond.notify_all();
        }

        virtual void communicatorInitialized(const shared_ptr<Ice::Communicator>& communicator)
        {
            lock_guard<mutex> lock(_mutex);
            _communicator = communicator;
        }

        void start()
        {
            _thread = move(thread([=] { run(); }));
        }

        void shutdown(JNIEnv*)
        {
            lock_guard<mutex> lock(_mutex);
            if(_communicator)
            {
                _communicator->shutdown();
            }
        }

        jstring getOutput(JNIEnv* env) const
        {
            lock_guard<mutex> lock(_mutex);
            return toJava(env, _out.str());
        }

        int waitReady(JNIEnv* env, int timeout) const
        {
            unique_lock<mutex> lock(_mutex);
            while(!_ready && !_completed)
            {
                if(_cond.wait_for(lock, chrono::seconds(timeout)) == cv_status::timeout)
                {
                    return -1;
                }
            }
            if(_completed && _status == EXIT_FAILURE)
            {
                return 1;
            }
            return 0;
        }

        int waitSuccess(JNIEnv* env, int timeout) const
        {
            unique_lock<mutex> lock(_mutex);
            while(!_completed)
            {
                if(_cond.wait_for(lock, chrono::seconds(timeout)) == cv_status::timeout)
                {
                    return -1;
                }
            }
            return _status;
        }

    private:

        void completed(int status)
        {
            lock_guard<mutex> lock(_mutex);
            _completed = true;
            _status = status;
            _communicator = 0;
            _cond.notify_all();
        }

        void run()
        {
            auto prefix = _testsuite;
            replace(prefix.begin(), prefix.end(), '/', '_');
            string libname = "lib" + prefix + "_" + _suffix + ".so";
            auto lib = dlopen(libname.c_str(), RTLD_NOW);
            if(!lib)
            {
                print(string("couldn't load library `" + libname + "': ") + dlerror());
                completed(1);
                return;
            }

            string name;
            if(_exe == "client")
            {
                name = "Client";
            }
            else if(_exe == "server")
            {
                name = "Server";
            }
            else if(_exe == "serveramd")
            {
                name = "ServerAMD";
            }
            else if(_exe == "collocated")
            {
                name = "Collocated";
            }
            else
            {
                print("couldn't find entry point for " + _exe);
                completed(1);
                return;
            }

            auto symname = "create" + name;
            auto sym = dlsym(lib, symname.c_str());
            if(!sym)
            {
                print(string("couldn't find `" + symname + "` entry point in `" + libname + "`: ") + dlerror());
                dlclose(lib);
                completed(1);
                return;
            }

            if(_exe.find("client") != string::npos || _exe.find("collocated") != string::npos)
            {
                streamRedirect.setControllerHelper(this);
            }

            CREATE_HELPER_ENTRY_POINT createHelper = (CREATE_HELPER_ENTRY_POINT)sym;
            char** argv = new char*[_args.size() + 2];
            argv[0] = const_cast<char*>(_exe.c_str());
            for(unsigned int i = 0; i < _args.size(); ++i)
            {
                argv[i + 1] = const_cast<char*>(_args[i].c_str());
            }
            argv[_args.size() + 1] = 0;
            try
            {
                unique_ptr<Test::TestHelper> helper(createHelper());
                helper->setControllerHelper(this);
                helper->run(static_cast<int>(_args.size() + 1), argv);
                completed(0);
            }
            catch(const std::exception& ex)
            {
                print("unexpected exception while running `" + _args[0] + "':\n" + ex.what());
                completed(1);
            }
            catch(...)
            {
                print("unexpected unknown exception while running `" + _args[0] + "'");
                completed(1);
            }
            delete[] argv;

            if(_exe.find("client") != string::npos || _exe.find("collocated") != string::npos)
            {
                streamRedirect.setControllerHelper(0);
            }

            dlclose(lib);
        }

        string _testsuite;
        string _suffix;
        string _exe;
        vector<string> _args;

        thread _thread;
        mutable mutex _mutex;
        mutable condition_variable _cond;
        bool _ready;
        bool _completed;
        int _status;
        std::ostringstream _out;
        std::shared_ptr<Ice::Communicator> _communicator;
    };
}

extern "C"
JNIEXPORT jlong JNICALL
Java_com_zeroc_testcontrollercpp_ControllerApp_00024ControllerHelperI_createControllerHelper(JNIEnv* env,
                                                                                             jobject,
                                                                                             jstring testsuite,
                                                                                             jstring suffix,
                                                                                             jstring exe,
                                                                                             jobjectArray args)
{
    auto helper = new ControllerHelperI(env, testsuite, suffix, exe, args);
    helper->start();
    return reinterpret_cast<jlong>(helper);
}

extern "C"
JNIEXPORT jstring JNICALL
Java_com_zeroc_testcontrollercpp_ControllerApp_00024ControllerHelperI_getOutput(JNIEnv* env,
                                                                                jobject,
                                                                                jlong helper)
{
    return reinterpret_cast<ControllerHelperI*>(helper)->getOutput(env);
}

extern "C"
JNIEXPORT void JNICALL
Java_com_zeroc_testcontrollercpp_ControllerApp_00024ControllerHelperI_shutdown(JNIEnv* env,
                                                                               jobject,
                                                                               jlong helper)
{
    reinterpret_cast<ControllerHelperI*>(helper)->shutdown(env);
}

extern "C"
JNIEXPORT jint JNICALL
Java_com_zeroc_testcontrollercpp_ControllerApp_00024ControllerHelperI_waitReady(JNIEnv* env,
                                                                                jobject,
                                                                                jlong helper,
                                                                                jint timeout)
{
    return reinterpret_cast<ControllerHelperI*>(helper)->waitReady(env, timeout);
}

extern "C"
JNIEXPORT jint JNICALL
Java_com_zeroc_testcontrollercpp_ControllerApp_00024ControllerHelperI_waitSuccess(JNIEnv* env,
                                                                                  jobject,
                                                                                  jlong helper,
                                                                                  jint timeout)
{
    return reinterpret_cast<ControllerHelperI*>(helper)->waitSuccess(env, timeout);
}
