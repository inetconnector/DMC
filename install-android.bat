@echo off
setlocal EnableExtensions EnableDelayedExpansion
goto :main

:resolve_adb
set "ADB="
if defined ADB_PATH if exist "%ADB_PATH%" set "ADB=%ADB_PATH%"
if not defined ADB if exist "%LOCALAPPDATA%\Android\Sdk\platform-tools\adb.exe" set "ADB=%LOCALAPPDATA%\Android\Sdk\platform-tools\adb.exe"
if not defined ADB if exist "%USERPROFILE%\AppData\Local\Android\Sdk\platform-tools\adb.exe" set "ADB=%USERPROFILE%\AppData\Local\Android\Sdk\platform-tools\adb.exe"
if not defined ADB if defined ANDROID_HOME if exist "%ANDROID_HOME%\platform-tools\adb.exe" set "ADB=%ANDROID_HOME%\platform-tools\adb.exe"
if not defined ADB if defined ANDROID_SDK_ROOT if exist "%ANDROID_SDK_ROOT%\platform-tools\adb.exe" set "ADB=%ANDROID_SDK_ROOT%\platform-tools\adb.exe"
if not defined ADB if exist "C:\Android\platform-tools\adb.exe" set "ADB=C:\Android\platform-tools\adb.exe"
if not defined ADB (
  where adb >nul 2>nul
  if not errorlevel 1 (
    for /f "usebackq delims=" %%A in (`where adb`) do (
      if not defined ADB set "ADB=%%A"
    )
  )
)
if not defined ADB (
  echo [ERROR] adb.exe not found.
  echo [HINT] Install Android SDK Platform-Tools or set ADB_PATH.
  exit /b 1
)
exit /b 0

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

:select_device
set "DEVICE_ID="
set /a DEVICE_COUNT=0
for /f "skip=1 tokens=1,2" %%A in ('"%ADB%" devices') do (
  if "%%B"=="device" (
    set /a DEVICE_COUNT+=1
    if not defined DEVICE_ID set "DEVICE_ID=%%A"
  )
)

if %DEVICE_COUNT% EQU 0 (
  echo [ERROR] No Android device detected.
  echo [HINT] Connect phone, enable USB debugging, accept RSA prompt.
  exit /b 1
)

if %DEVICE_COUNT% GTR 1 (
  if defined TARGET_DEVICE_ID (
    set "DEVICE_ID=%TARGET_DEVICE_ID%"
  ) else (
    echo [ERROR] Multiple devices detected. Set TARGET_DEVICE_ID and run again.
    "%ADB%" devices
    exit /b 1
  )
)

"%ADB%" -s "%DEVICE_ID%" wait-for-device >nul 2>nul
exit /b 0

:main
for %%I in ("%~dp0.") do set "ROOT=%%~fI"
set "APP_ID=com.inetconnector.aichat"
set "ANDROID_DIR=%ROOT%\android\llama.android"
set "WRAPPER_JAR=%ANDROID_DIR%\gradle\wrapper\gradle-wrapper.jar"
set "OUTPUT_DEBUG_APK=%ANDROID_DIR%\app\build\outputs\apk\debug\app-debug.apk"
set "OUTPUT_RELEASE_APK=%ANDROID_DIR%\app\build\outputs\apk\release\app-release.apk"
set "OUTPUT_RELEASE_UNSIGNED_APK=%ANDROID_DIR%\app\build\outputs\apk\release\app-release-unsigned.apk"
set "VARIANT=debug"

if /I "%APP_INSTALL_VARIANT%"=="release" set "VARIANT=release"

echo ============================================================
echo Local Android Install - %APP_ID%
echo ============================================================
echo Root: %ROOT%
echo Variant: %VARIANT%

if not exist "%WRAPPER_JAR%" (
  echo [ERROR] Missing Gradle wrapper jar: %WRAPPER_JAR%
  goto :fail
)

call :resolve_adb
if errorlevel 1 goto :fail

call :resolve_java
if errorlevel 1 goto :fail

echo [INFO] Using ADB: %ADB%
echo [INFO] Starting ADB server...
"%ADB%" start-server >nul 2>nul
if errorlevel 1 (
  echo [ERROR] Could not start ADB server.
  goto :fail
)

call :select_device
if errorlevel 1 goto :fail

echo [INFO] Building Android app...
pushd "%ANDROID_DIR%"
if /I "%VARIANT%"=="release" (
  "%JAVA_EXE%" -classpath gradle\wrapper\gradle-wrapper.jar org.gradle.wrapper.GradleWrapperMain :app:clean :app:assembleRelease --console=plain
) else (
  "%JAVA_EXE%" -classpath gradle\wrapper\gradle-wrapper.jar org.gradle.wrapper.GradleWrapperMain :app:clean :app:assembleDebug --console=plain
)
set "BUILD_EXIT=%ERRORLEVEL%"
popd

if not "%BUILD_EXIT%"=="0" (
  echo [ERROR] Gradle build failed with code %BUILD_EXIT%.
  goto :fail
)

set "APK_PATH="
if /I "%VARIANT%"=="release" (
  if exist "%OUTPUT_RELEASE_APK%" set "APK_PATH=%OUTPUT_RELEASE_APK%"
  if not defined APK_PATH if exist "%OUTPUT_RELEASE_UNSIGNED_APK%" set "APK_PATH=%OUTPUT_RELEASE_UNSIGNED_APK%"
) else (
  if exist "%OUTPUT_DEBUG_APK%" set "APK_PATH=%OUTPUT_DEBUG_APK%"
)

if not defined APK_PATH (
  echo [ERROR] APK not found after build.
  if /I "%VARIANT%"=="release" (
    echo [INFO] Checked:
    echo        %OUTPUT_RELEASE_APK%
    echo        %OUTPUT_RELEASE_UNSIGNED_APK%
  ) else (
    echo [INFO] Checked:
    echo        %OUTPUT_DEBUG_APK%
  )
  goto :fail
)

echo [INFO] Installing APK on device %DEVICE_ID%...
"%ADB%" -s "%DEVICE_ID%" install -r -d "%APK_PATH%"
if errorlevel 1 (
  echo [ERROR] APK install failed.
  goto :fail
)

echo [INFO] Granting runtime permissions...
"%ADB%" -s "%DEVICE_ID%" shell pm grant %APP_ID% android.permission.CAMERA >nul 2>nul
"%ADB%" -s "%DEVICE_ID%" shell pm grant %APP_ID% android.permission.RECORD_AUDIO >nul 2>nul
"%ADB%" -s "%DEVICE_ID%" shell pm grant %APP_ID% android.permission.READ_MEDIA_IMAGES >nul 2>nul
"%ADB%" -s "%DEVICE_ID%" shell pm grant %APP_ID% android.permission.READ_MEDIA_VIDEO >nul 2>nul
"%ADB%" -s "%DEVICE_ID%" shell pm grant %APP_ID% android.permission.READ_MEDIA_AUDIO >nul 2>nul

echo [INFO] Launching app...
"%ADB%" -s "%DEVICE_ID%" shell monkey -p %APP_ID% -c android.intent.category.LAUNCHER 1 >nul
if errorlevel 1 (
  echo [WARN] Could not launch app with monkey.
)

echo.
echo [OK] Installed and started successfully.
echo [OK] Device: %DEVICE_ID%
echo [OK] APK: %APK_PATH%
goto :eof

:fail
echo.
echo [FAILED]
pause
exit /b 1
