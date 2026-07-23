@echo off
setlocal EnableExtensions EnableDelayedExpansion
goto :main

:resolve_java
set "JAVA_EXE="
if defined JAVA_HOME if exist "%JAVA_HOME%\bin\java.exe" set "JAVA_EXE=%JAVA_HOME%\bin\java.exe"
if not defined JAVA_EXE if exist "%ProgramFiles%\Android\openjdk\jdk-21.0.8\bin\java.exe" set "JAVA_EXE=%ProgramFiles%\Android\openjdk\jdk-21.0.8\bin\java.exe"
if not defined JAVA_EXE if exist "%ProgramFiles%\Android\Android Studio\jbr\bin\java.exe" set "JAVA_EXE=%ProgramFiles%\Android\Android Studio\jbr\bin\java.exe"
if not defined JAVA_EXE (
  where java >nul 2>nul
  if not errorlevel 1 (
    for /f "usebackq delims=" %%A in (`where java`) do (
      if not defined JAVA_EXE set "JAVA_EXE=%%A"
    )
  )
)
if not defined JAVA_EXE (
  echo [ERROR] Java runtime not found.
  echo [HINT] Set JAVA_HOME or install JDK 17+ / Android Studio.
  exit /b 1
)
exit /b 0

:resolve_vulkan_sdk
if defined VULKAN_SDK if exist "%VULKAN_SDK%\Bin\glslc.exe" (
  set "PATH=%VULKAN_SDK%\Bin;%PATH%"
  echo [INFO] Using Vulkan SDK: %VULKAN_SDK%
  exit /b 0
)

set "VULKAN_SDK="
for /f "delims=" %%D in ('dir /b /ad /o-n "C:\VulkanSDK" 2^>nul') do (
  if not defined VULKAN_SDK if exist "C:\VulkanSDK\%%D\Bin\glslc.exe" set "VULKAN_SDK=C:\VulkanSDK\%%D"
)

if defined VULKAN_SDK (
  set "PATH=%VULKAN_SDK%\Bin;%PATH%"
  echo [INFO] Using Vulkan SDK: %VULKAN_SDK%
) else (
  echo [WARN] Vulkan SDK not found; Android build will fall back to CPU/OpenCL if available.
)
exit /b 0

:resolve_gradle
set "GRADLE_EXE="
for /f "delims=" %%D in ('dir /b /ad /o-n "%USERPROFILE%\.gradle\wrapper\dists\gradle-8.14.3-bin" 2^>nul') do (
  if not defined GRADLE_EXE if exist "%USERPROFILE%\.gradle\wrapper\dists\gradle-8.14.3-bin\%%D\gradle-8.14.3\bin\gradle.bat" set "GRADLE_EXE=%USERPROFILE%\.gradle\wrapper\dists\gradle-8.14.3-bin\%%D\gradle-8.14.3\bin\gradle.bat"
)
if not defined GRADLE_EXE if exist "%ProgramFiles%\Gradle\gradle-8.14.3\bin\gradle.bat" set "GRADLE_EXE=%ProgramFiles%\Gradle\gradle-8.14.3\bin\gradle.bat"
if not defined GRADLE_EXE if exist "%ProgramFiles(x86)%\Gradle\gradle-8.14.3\bin\gradle.bat" set "GRADLE_EXE=%ProgramFiles(x86)%\Gradle\gradle-8.14.3\bin\gradle.bat"
if defined GRADLE_EXE (
  echo [INFO] Using local Gradle: %GRADLE_EXE%
) else (
  echo [WARN] Local Gradle not found; falling back to the Gradle wrapper.
)
exit /b 0

:resolve_vsdevcmd
set "VSDEVCMD="
if exist "%ProgramFiles%\Microsoft Visual Studio\18\Community\Common7\Tools\VsDevCmd.bat" set "VSDEVCMD=%ProgramFiles%\Microsoft Visual Studio\18\Community\Common7\Tools\VsDevCmd.bat"
if not defined VSDEVCMD if exist "%ProgramFiles(x86)%\Microsoft Visual Studio\18\Community\Common7\Tools\VsDevCmd.bat" set "VSDEVCMD=%ProgramFiles(x86)%\Microsoft Visual Studio\18\Community\Common7\Tools\VsDevCmd.bat"
if defined VSDEVCMD (
  call "%VSDEVCMD%" -no_logo -arch=x64 -host_arch=x64 >nul
  if errorlevel 1 (
    echo [ERROR] Visual Studio developer environment could not be activated.
    exit /b 1
  )
  echo [INFO] Activated Visual Studio developer environment for Vulkan shader generation.
) else (
  echo [WARN] Visual Studio developer environment not found; Vulkan shader generation may fail.
)
exit /b 0

:write_vulkan_host_toolchain
set "HOST_TOOLCHAIN_FILE=%TEMP%\ai-chat-vulkan-host-toolchain.cmake"
set "CL_EXE="
for /f "delims=" %%C in ('where cl') do (
  if not defined CL_EXE set "CL_EXE=%%C"
)
if not defined CL_EXE (
  echo [ERROR] cl.exe not found after activating Visual Studio.
  exit /b 1
)
set "CL_CMAKE=%CL_EXE:\=/%"

break > "%HOST_TOOLCHAIN_FILE%"
>> "%HOST_TOOLCHAIN_FILE%" echo set(CMAKE_BUILD_TYPE Release)
>> "%HOST_TOOLCHAIN_FILE%" echo set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
>> "%HOST_TOOLCHAIN_FILE%" echo set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY NEVER)
>> "%HOST_TOOLCHAIN_FILE%" echo set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE NEVER)
>> "%HOST_TOOLCHAIN_FILE%" echo set(CMAKE_OBJECT_PATH_MAX 128)
>> "%HOST_TOOLCHAIN_FILE%" echo set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
>> "%HOST_TOOLCHAIN_FILE%" echo set(CMAKE_C_COMPILER "%CL_CMAKE%")
>> "%HOST_TOOLCHAIN_FILE%" echo set(CMAKE_CXX_COMPILER "%CL_CMAKE%")

echo [INFO] Wrote Vulkan host toolchain: %HOST_TOOLCHAIN_FILE%
exit /b 0

:wait_for_file
set "WAIT_FILE=%~1"
set "WAIT_SECONDS=%~2"
if not defined WAIT_SECONDS set "WAIT_SECONDS=10"
set /a WAIT_COUNT=0
:wait_for_file_loop
if exist "%WAIT_FILE%" exit /b 0
set /a WAIT_COUNT+=1
if %WAIT_COUNT% GEQ %WAIT_SECONDS% exit /b 1
timeout /t 1 /nobreak >nul
goto :wait_for_file_loop

:main
for %%I in ("%~dp0.") do set "ROOT=%%~fI"
set "APP_ID=com.inetconnector.dmc"
set "APP_VERSION_NAME=1.0.1"
set "APP_VERSION_CODE=2"
set "ANDROID_DIR=%ROOT%\android\llama.android"
set "WRAPPER_JAR=%ANDROID_DIR%\gradle\wrapper\gradle-wrapper.jar"
set "VARIANT=debug"
set "PUBLISH_DIR=%ROOT%\publish\%APP_ID%\%APP_VERSION_NAME%+%APP_VERSION_CODE%"
set "GRADLE_OFFLINE_ARGS="
if /I "%ANDROID_BUILD_OFFLINE%"=="1" set "GRADLE_OFFLINE_ARGS=--offline"

if /I "%ANDROID_BUILD_VARIANT%"=="release" set "VARIANT=release"

echo ============================================================
echo Build Android APK - InetMind Local AI
echo ============================================================
echo Root: %ROOT%
echo Android project: %ANDROID_DIR%
echo Variant: %VARIANT%

powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%ROOT%\scripts\windows\prepare-llama-submodule.ps1" -RepositoryRoot "%ROOT%"
if errorlevel 1 (
  echo [ERROR] Pinned llama.cpp submodule preparation failed.
  exit /b 1
)

if not exist "%WRAPPER_JAR%" (
  echo [ERROR] Missing Gradle wrapper jar: %WRAPPER_JAR%
  exit /b 1
)

if /I "%ANDROID_SKIP_WEBUI_BUILD%"=="1" (
  echo [WARN] Skipping Android Web UI rebuild because ANDROID_SKIP_WEBUI_BUILD=1.
) else (
  powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%ROOT%\scripts\windows\prepare-android-webui.ps1" -RepositoryRoot "%ROOT%"
  if errorlevel 1 (
    echo [ERROR] Android Web UI build failed.
    exit /b 1
  )
)

call :resolve_vulkan_sdk
if errorlevel 1 exit /b 1

call :resolve_gradle
if errorlevel 1 exit /b 1

call :resolve_vsdevcmd
if errorlevel 1 exit /b 1

call :write_vulkan_host_toolchain
if errorlevel 1 exit /b 1

call :resolve_java
if errorlevel 1 exit /b 1

pushd "%ANDROID_DIR%"
if /I "%VARIANT%"=="release" (
  if defined GRADLE_EXE (
    call "%GRADLE_EXE%" --no-daemon %GRADLE_OFFLINE_ARGS% -PaiChatHostToolchainFile=%HOST_TOOLCHAIN_FILE% :app:clean :app:assembleRelease :app:bundleRelease --console=plain
  ) else (
    "%JAVA_EXE%" -classpath gradle\wrapper\gradle-wrapper.jar org.gradle.wrapper.GradleWrapperMain --no-daemon %GRADLE_OFFLINE_ARGS% -PaiChatHostToolchainFile=%HOST_TOOLCHAIN_FILE% :app:clean :app:assembleRelease :app:bundleRelease --console=plain
  )
) else (
  if defined GRADLE_EXE (
    call "%GRADLE_EXE%" --no-daemon %GRADLE_OFFLINE_ARGS% -PaiChatHostToolchainFile=%HOST_TOOLCHAIN_FILE% :app:clean :app:assembleDebug --console=plain
  ) else (
    "%JAVA_EXE%" -classpath gradle\wrapper\gradle-wrapper.jar org.gradle.wrapper.GradleWrapperMain --no-daemon %GRADLE_OFFLINE_ARGS% -PaiChatHostToolchainFile=%HOST_TOOLCHAIN_FILE% :app:clean :app:assembleDebug --console=plain
  )
)
set "BUILD_EXIT=%ERRORLEVEL%"
popd

if not "%BUILD_EXIT%"=="0" (
  echo [ERROR] Gradle build failed with code %BUILD_EXIT%.
  exit /b %BUILD_EXIT%
)

if /I "%VARIANT%"=="release" (
  set "APK_PATH=%ANDROID_DIR%\app\build\outputs\apk\release\app-release.apk"
  call :wait_for_file "!APK_PATH!" 15
  if not exist "!APK_PATH!" set "APK_PATH=%ANDROID_DIR%\app\build\outputs\apk\release\app-release-unsigned.apk"
) else (
  set "APK_PATH=%ANDROID_DIR%\app\build\outputs\apk\debug\app-debug.apk"
)

if not exist "%APK_PATH%" (
  echo [ERROR] APK not found after build.
  echo [INFO] Checked: %APK_PATH%
  exit /b 1
)

powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%ROOT%\scripts\windows\verify-android-dmc.ps1" -ApkPath "%APK_PATH%"
if errorlevel 1 (
  echo [ERROR] APK does not contain the required DMC runtime.
  exit /b 1
)

if not exist "%PUBLISH_DIR%" mkdir "%PUBLISH_DIR%" >nul 2>nul
if /I "%VARIANT%"=="release" (
  if exist "%ANDROID_DIR%\app\build\outputs\bundle\release\app-release.aab" (
    copy /Y "%ANDROID_DIR%\app\build\outputs\bundle\release\app-release.aab" "%PUBLISH_DIR%\%APP_ID%-%APP_VERSION_NAME%+%APP_VERSION_CODE%-release.aab" >nul
  )
)
copy /Y "%APK_PATH%" "%PUBLISH_DIR%\%APP_ID%-%APP_VERSION_NAME%+%APP_VERSION_CODE%-%VARIANT%.apk" >nul

echo.
echo [OK] Build completed.
echo [OK] APK: %APK_PATH%
echo [OK] Publish: %PUBLISH_DIR%
goto :eof
