# Flutter IDE Setup for IntelliJ IDEA

## Prerequisites

1. **Flutter SDK** is installed at: `D:\java\flutter`
2. **Dart SDK** is bundled with Flutter at: `D:\java\flutter\bin\cache\dart-sdk`
3. **IntelliJ IDEA** with Flutter and Dart plugins installed

## Quick Setup

### Option 1: Run Setup Script

Run the provided batch script:
```bash
setup-flutter-ide.bat
```

This will configure the project and show you the next steps.

### Option 2: Manual Setup

1. **Configure Flutter SDK in IntelliJ**
   - Open IntelliJ IDEA
   - Go to: `File -> Settings -> Languages & Frameworks -> Flutter`
   - Set Flutter SDK path to: `D:\java\flutter`
   - Click `Apply`

2. **Configure Dart SDK** (Usually auto-detected)
   - Go to: `File -> Settings -> Languages & Frameworks -> Dart`
   - Dart SDK path should be: `D:\java\flutter\bin\cache\dart-sdk`
   - Enable Dart support for the project
   - Click `Apply` and `OK`

3. **Verify Plugin Installation**
   - Go to: `File -> Settings -> Plugins`
   - Ensure these plugins are installed and enabled:
     - Flutter
     - Dart
   - If not installed, search for them in the Marketplace and install

4. **Restart IntelliJ IDEA**
   - Close the project completely
   - Restart IntelliJ IDEA
   - Reopen the project

5. **Verify Run Configurations**
   - Look for the run configuration dropdown in the toolbar
   - You should see these configurations:
     - SIMS App - Debug
     - SIMS App - Profile
     - SIMS App - Release
     - SIMS App - Tests
     - SIMS App - Build APK
     - SIMS App - Flutter Doctor
     - SIMS App - Flutter Clean

## Troubleshooting

### Run Configurations Not Showing

If the run configurations don't appear:

1. **Check Flutter SDK Path**
   ```
   File -> Settings -> Languages & Frameworks -> Flutter
   Ensure the path is set to: D:\java\flutter
   ```

2. **Invalidate Caches**
   ```
   File -> Invalidate Caches -> Invalidate and Restart
   ```

3. **Reimport Project**
   - Close IntelliJ
   - Delete `.idea` folder (backup first if you've made custom changes)
   - Reopen IntelliJ and open the project
   - IntelliJ will recreate the `.idea` folder

4. **Run Flutter Commands Manually**
   ```bash
   cd sims-app
   flutter pub get
   flutter doctor -v
   ```

### Flutter Plugin Not Recognized

If IntelliJ doesn't recognize the Flutter project:

1. Right-click on `sims-app` folder
2. Select `Flutter -> Open Android module in Android Studio` (this triggers Flutter recognition)
3. Or select `Flutter -> Flutter Pub Get`

### Dart Analysis Not Working

1. Go to `File -> Settings -> Languages & Frameworks -> Dart`
2. Check "Enable Dart support for the project"
3. Set the scope to include the `sims-app` folder
4. Click `Apply`

## Running the App

### From IntelliJ

1. Connect an Android device or start an emulator
2. Select `SIMS App - Debug` from the run configuration dropdown
3. Click the green play button (Run) or bug icon (Debug)

### From Command Line

```bash
cd sims-app
flutter run
```

### Building APK

From IntelliJ:
- Select `SIMS App - Build APK` and click Run

From command line:
```bash
cd sims-app
flutter build apk --release
```

Output will be at: `sims-app/build/app/outputs/flutter-apk/app-release.apk`

## SDK Paths Reference

- **Flutter SDK**: `D:\java\flutter`
- **Dart SDK**: `D:\java\flutter\bin\cache\dart-sdk`
- **Android SDK**: `C:\Users\pp\AppData\Local\Android\sdk` (if Android Studio is installed)

## Project Structure

```
sims-bw/
├── .idea/                    # IntelliJ IDEA configuration
│   ├── runConfigurations/   # Run configurations
│   ├── libraries/           # SDK library definitions
│   └── misc.xml             # Flutter SDK path
├── sims-app/                # Flutter application
│   ├── lib/                 # Dart source code
│   ├── test/                # Tests
│   ├── android/             # Android platform code
│   └── pubspec.yaml         # Flutter dependencies
└── setup-flutter-ide.bat    # Quick setup script
```

## Next Steps

After setup is complete:

1. Open `sims-app/lib/main.dart` in IntelliJ
2. You should see code completion and no errors
3. Run `SIMS App - Debug` to test the app
4. Use hot reload (⚡) during development for instant changes

## Additional Resources

- Flutter Documentation: https://docs.flutter.dev/
- IntelliJ Flutter Plugin: https://plugins.jetbrains.com/plugin/9212-flutter
- SIMS App Documentation: See `CLAUDE.md` for project-specific guidance
