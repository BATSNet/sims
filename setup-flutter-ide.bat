@echo off
echo Setting up Flutter project for IntelliJ IDEA...
echo.

cd sims-app

echo Step 1: Running flutter pub get...
flutter pub get
if %errorlevel% neq 0 (
    echo ERROR: flutter pub get failed!
    pause
    exit /b 1
)
echo.

echo Step 2: Running flutter doctor...
flutter doctor -v
echo.

echo Step 3: Generating Flutter configuration files...
flutter packages get
echo.

echo ============================================
echo Setup complete!
echo.
echo Next steps:
echo 1. Restart IntelliJ IDEA if it's currently open
echo 2. In IntelliJ: File -^> Settings -^> Languages ^& Frameworks -^> Flutter
echo 3. Set Flutter SDK path to: D:\java\flutter
echo 4. Set Dart SDK path to: D:\java\flutter\bin\cache\dart-sdk
echo 5. Click Apply and OK
echo 6. Close and reopen the project
echo 7. You should now see the run configurations in the toolbar
echo ============================================
echo.
pause
