# Building m8c for iPhone and iPad

## Generate and run

Requirements: macOS, full Xcode with an iOS SDK, Git, and XcodeGen 2.46.0 or newer.
Install a simulator runtime through Xcode to run the simulator smoke checks.

```sh
brew install xcodegen
make xcode-ios
open m8c-ios.xcodeproj
```

If command-line tools are selected instead of full Xcode, set `DEVELOPER_DIR`
to your installation before running these commands. For example:

```sh
export DEVELOPER_DIR=/Applications/Xcode-beta.app/Contents/Developer
```

Choose the shared `m8c-ios` scheme and an iPhone, iPad, or simulator. The app
supports iOS 15.6 and newer. The simulator can validate launch and rendering;
USB MIDI communication and audio monitoring need a physical device and M8.
The RtMidi backend requires M8 firmware 6.0.0 or newer.

The first generation initializes `ext/SDL` at its recorded Git submodule commit
(SDL 3.4.16). Later runs use that checkout without fetching a moving branch.
Generation rejects a different checkout revision, tracked SDL edits, or conflicting
SDL build configuration instead of overwriting them.

Rerun `make xcode-ios` after adding/removing source files or changing `project.yml`.
The generated project is ignored; edit the spec and packaging files, rather than
the generated project. Normal Xcode builds do not fetch dependencies.

## Signing and versions

Copy App.xcconfig to `package/ios/Local.xcconfig` and write your own settings for the package id/version etc.

This optional file is ignored and survives regeneration. 

## Command-line validation and archiving

Unsigned builds require no development team:

```sh
xcodebuild -project m8c-ios.xcodeproj -scheme m8c-ios \
  -configuration Debug -destination 'generic/platform=iOS Simulator' \
  -derivedDataPath build/ios CODE_SIGNING_ALLOWED=NO build
xcodebuild -project m8c-ios.xcodeproj -scheme m8c-ios \
  -configuration Release -destination 'generic/platform=iOS' \
  -derivedDataPath build/ios CODE_SIGNING_ALLOWED=NO build
```

Repeat with the other configuration for both platforms. To create a signed
archive after configuring your team:

```sh
xcodebuild -project m8c-ios.xcodeproj -scheme m8c-ios \
  -configuration Release -destination 'generic/platform=iOS' \
  -archivePath build/m8c-ios.xcarchive archive
```

Alternatively choose Product > Archive in Xcode. An unsigned archive can be
inspected by appending `CODE_SIGNING_ALLOWED=NO` to the command; it cannot be
installed directly on a device. Export/distribution is handled through Xcode's
Organizer with your own signing settings.

Verify that the app contains `Frameworks/SDL3.framework`, `Assets.car`, the
compiled launch storyboard, and a plain `gamecontrollerdb.txt`. The archive
should install only the app; SDL is embedded, with `SKIP_INSTALL=YES` provided
by the generated `ext/SDL/Xcode/SDL/build.xcconfig`. It also sets SDL's minimum
iOS version to 15.6, since newer SDKs reject its upstream iOS 11 default.
This file is ignored through
the submodule's local Git exclude file and does not modify upstream project files.

For a smoke check, launch on iPhone and iPad simulators and check the logo fits
the screen, followed by the app's waiting screen. On hardware, check M8 display,
audio monitoring, gamepad input, rotation, and background/foreground transitions.

## Updating SDL and resources

To upgrade SDL, fetch its tags, check out the desired stable release under
`ext/SDL`, and stage that submodule path with `git add ext/SDL`. Then regenerate
and repeat simulator/device builds and archive checks.