@echo off
setlocal EnableExtensions
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

:main
for %%I in ("%~dp0.") do set "ROOT=%%~fI"
set "ANDROID_DIR=%ROOT%\android\llama.android"
set "WRAPPER_JAR=%ANDROID_DIR%\gradle\wrapper\gradle-wrapper.jar"
set "VARIANT=debug"

if /I "%ANDROID_BUILD_VARIANT%"=="release" set "VARIANT=release"

echo ============================================================
echo Build Android APK - DMC AI Chat
echo ============================================================
echo Root: %ROOT%
echo Android project: %ANDROID_DIR%
echo Variant: %VARIANT%

if not exist "%WRAPPER_JAR%" (
  echo [ERROR] Missing Gradle wrapper jar: %WRAPPER_JAR%
  exit /b 1
)

call :resolve_java
if errorlevel 1 exit /b 1

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
  exit /b %BUILD_EXIT%
)

if /I "%VARIANT%"=="release" (
  set "APK_PATH=%ANDROID_DIR%\app\build\outputs\apk\release\app-release.apk"
  if not exist "%APK_PATH%" set "APK_PATH=%ANDROID_DIR%\app\build\outputs\apk\release\app-release-unsigned.apk"
) else (
  set "APK_PATH=%ANDROID_DIR%\app\build\outputs\apk\debug\app-debug.apk"
)

if not exist "%APK_PATH%" (
  echo [ERROR] APK not found after build.
  echo [INFO] Checked: %APK_PATH%
  exit /b 1
)

echo.
echo [OK] Build completed.
echo [OK] APK: %APK_PATH%
goto :eof
