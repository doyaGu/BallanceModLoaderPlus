# Ballance Mod Loader Plus

## Overview

Ballance Mod Loader Plus is a renovated Mod Loader for Ballance. Based on Gamepiaynmo's BallanceModLoader, this new version brings some enhanced features and improved stability.

## Features

- **Brand New UI**: Modern user interface based on ImGui for a more intuitive and responsive user experience.
- **Optimized FPS**: Optimized performance for smoother gameplay and higher frame rates.
- **Unicode Support**: Support for Unicode, allowing mods and user interfaces to include characters from a wide range of languages.
- **Enhanced Stability**: Improved codebase for better performance and fewer crashes.
- **Regular Updates**: Ongoing updates to add new features and fix issues.

## Environment

Supports Windows 7, 8, 8.1, 10, 11.

## Instructions

### Installation

1. Download the latest build from [releases page](https://github.com/doyaGu/BallanceModLoaderPlus/releases).
2. Extract the downloaded ZIP file to a location of your choice.
3. Copy all the extracted files to the root directory of your Ballance game installation. After that, you should find the following files in your Ballance directory:
   - `BMLPlus.dll` in the `BuildingBlocks` directory.
   - `ModLoader` directory.
4. Start the game by clicking on `Player.exe`.

### Uninstalling

Go to the Ballance root folder and delete `BMLPlus.dll` in the `BuildingBlocks` directory. If you would like to remove the entire data of BML (installed mods, maps, configs, etc.), delete the `ModLoader` folder as well.

## Frequently Asked Questions (FAQ)

### Q1: How can I confirm if BML+ is installed correctly?

After launching the game, you should see the BML message at the top of the game UI. If you do not see this message, please check if the files are correctly placed in the root directory of the game and original BML or older version of BML+ is removed.

### Q2: What should I do if BML+ is not loaded?

If BML+ is not loaded, try the following steps:

- Ensure that `BMLPlus.dll` is correctly placed in the `BuildingBlocks` directory.
- Check if there are any conflicting files from previous versions of BML or other mods.
- Make sure you have the [Visual C++ Redistributable for Visual Studio 2015-2022](https://learn.microsoft.com/en-us/cpp/windows/latest-supported-vc-redist?view=msvc-170) installed on your system.
- If the problem persists, open an issue on GitHub.

### Q3: What should I do if my game crashes after installing BML?

Ensure that you followed the installation instructions correctly. If the problem persists, please open an issue on GitHub and provide the following information:

- A detailed description of the problem you encountered.
- Screenshots to help explain the issue, if possible.
- The content of the `ModLoader.log` file (found in the `ModLoader` directory).

### Q4: How do I update to a new version of BML+?

Download the latest version and replace the old files according to the installation steps.

### Q5: Is BML+ compatible with mods for the original BML (mods with extension .bmod)?

No, BML+ is not compatible with mods created for the original BML that have the `.bmod` extension.

### Q6: Is BML+ compatible with the original player?

No, BML+ is built for the [New Player](https://github.com/doyaGu/BallancePlayer) and cannot support the original player properly.

## License

This project is licensed under the MIT License. See the LICENSE file for more details.

## Acknowledgments

- Special thanks to Gamepiaynmo for the original BallanceModLoader.
- Thanks to the Ballance modding community for their continuous support and contributions.

## Contact

For any questions or support, please reach out via the GitHub [issues page](https://github.com/doyaGu/BallanceModLoaderPlus/issues).
