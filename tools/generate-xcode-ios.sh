#!/bin/sh
set -eu

cd "$(dirname "$0")/.."

for tool in git xcodegen xcodebuild; do
    if ! command -v "$tool" >/dev/null 2>&1; then
        echo "error: $tool is required; see package/ios/README.md" >&2
        exit 1
    fi
done
if ! xcodebuild -version >/dev/null 2>&1; then
    echo "error: select a full Xcode installation with DEVELOPER_DIR or xcode-select" >&2
    exit 1
fi

# Only initialize a missing checkout. Never reset an existing dependency.
if [ ! -e ext/SDL/.git ]; then
    git submodule update --init --recursive -- ext/SDL
fi
expected=$(git rev-parse :ext/SDL)
actual=$(git -C ext/SDL rev-parse HEAD)
if [ "$actual" != "$expected" ]; then
    echo "error: ext/SDL differs from the pinned revision; review it before updating the submodule" >&2
    exit 1
fi
if ! git -C ext/SDL diff --quiet HEAD --; then
    echo "error: ext/SDL has tracked edits; preserve or resolve them before generating" >&2
    exit 1
fi

# SDL's upstream project offers this optional configuration hook.
sdl_config=ext/SDL/Xcode/SDL/build.xcconfig
if [ -e "$sdl_config" ] && ! cmp -s package/ios/SDL.xcconfig "$sdl_config"; then
    echo "error: $sdl_config contains local settings; refusing to overwrite it" >&2
    exit 1
fi
exclude=$(git -C ext/SDL rev-parse --absolute-git-dir)/info/exclude
mkdir -p "$(dirname "$exclude")"
if ! grep -qxF '/Xcode/SDL/build.xcconfig' "$exclude" 2>/dev/null; then
    printf '\n/Xcode/SDL/build.xcconfig\n' >> "$exclude"
fi
if [ ! -e "$sdl_config" ]; then
    cp package/ios/SDL.xcconfig "$sdl_config"
fi

xcodegen generate --spec project.yml
echo "Open m8c-ios.xcodeproj and select the m8c-ios scheme."
