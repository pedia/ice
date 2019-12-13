# Building Ice for C++ with the Android NDK

This file describes how to build Ice for C++ from source using the Android NDK
and how to test the resulting build.

## Android Build Requirements

Building Ice for Android NDK requires the Android SDK build tools with the NDK
and cmake installed. We tested with the following components:

- Android Studio 3.5
- Android SDK 29

Using Ice's Java mapping with Java 8 requires at minimum API level 24:

- Android 7 (API24)

## Building Ice for C++

The build system requires the Slice to C++ compiler from Ice for C++. If you
have not built Ice for C++ in this source distribution, you must set the
`ICE_BIN_DIST` environment variable to `cpp` and the `ICE_HOME` environment
variable with the path name of your Ice installation. For example, on Linux with
an RPM installation:
```
export ICE_BIN_DIST=cpp
export ICE_HOME=/usr
```

On Windows with an MSI installation:
```
set ICE_BIN_DIST=cpp
set ICE_HOME=C:\Program Files\ZeroC\Ice-3.7.3
```

Before building Ice for Java, review the settings in the file
`gradle.properties` and edit as necessary.

To build Ice for C++ libraries, run
```
gradlew build
```

Upon completion, the Ice libraries are placed in the `lib` subdirectory. The
build static and shared libraries in debug and release mode for each Android
supported ABI (x86, x86_64, armeabi-v7a, arm64-v8a).

If at any time you wish to discard the current build and start a new one, use
these commands:
```
gradlew clean
gradlew build
```

## Building the Ice for Android Tests

The `../test/android/controller` directory contains an Android Studio project
for the Ice test suite controller.

### Building the Android Test Controller

You must first build Ice for C++ according to the instructions above, then
follow these steps:

1. Start Android Studio
2. Select "Open an existing Android Studio project"
3. Navigate to and select the "java/test/android/controller" subdirectory
4. Click OK and wait for the project to open and build

### Running the Android Test Suite

The Android Studio project contains a `controller` app for the Ice test
suite. Prior to running the app, you must disable Android Studio's Instant Run
feature, located in File / Settings / Build, Execution, Deployment /
Instant Run.

Tests are started from the dev machine using the `allTests.py` script, similar
to the other language mappings. The script uses Ice for Python to communicate
with the Android app, therefore you must build the [Python mapping]
(../python) before continuing.

You also need to add the `tools\bin`, `platform-tools` and `emulator`
directories from the Android SDK to your PATH. On macOS, you can use the
following commands:

```
export PATH=~/Library/Android/sdk/tools/bin:$PATH
export PATH=~/Library/Android/sdk/platform-tools:$PATH
export PATH=~/Library/Android/sdk/emulator:$PATH
```

On Windows, you can use the following commands:

```
set PATH=%LOCALAPPDATA%\Android\sdk\tools\bin;%PATH%
set PATH=%LOCALAPPDATA%\Android\sdk\platform-tools;%PATH%
set PATH=%LOCALAPPDATA%\Android\sdk\emulator;%PATH%
```

Run the tests with the Android emulator by running the following command:

```
python allTests.py --android --controller-app
```

To run the tests on a Android device connected through USB, you can use
the `--device=usb` option as shown below:

```
python allTests.py --android --device=usb --controller-app
```

To connect to an Android device that is running adb you can use the
`--device=<ip-address>`

```
python allTests.py --android --device=<ip-address> --controller-app
```

To run the tests against a `controller` application started from Android
Studio you should omit the `--controller-app` option from the commands above.
