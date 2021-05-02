# USBToolBoxᵇᵉᵗᵃ

*Making USB mapping simple(r)*

The USBToolBox kext is a kext intended to make common actions for USB mapping easier.

## Features

* Attach to the controller instance or parent device, allowing for more ways to match
* Ignore port definitions from ACPI to force macOS to enumerate all ports manually
  * Bypasses borked ACPI as seen on some Ryzen motherboards and 400 series Intel motherboards
  * Replaces SSDT-RHUB
* Override any built-in Apple USB maps attaching based on SMBIOS and controller name
  * Removes the need for controller renames in ACPI patches
* Does not require model identifier specified in USB map (if attaching to PCI device)
* Very compatible with existing USB maps (port format is the same)
* Does not hardcode any port maps, unlike USBInjectAll

This does **not** patch the port limit.

## Configuration

USBToolBox supports configuration using boot arguments, properties, or in the map. You can set the properties on either the PCI device or the `AppleUSBHostController` instance.

Properties can be any type and only existence, not type, is checked, unless otherwise specified.

* `-utboff` (property `utb-off`): Disable USBToolBox completely

* `-utbacpioff` (property `utb-acpi-off`): Disable RHUB removal from ACPI plane (borked ACPI removal)

* `-utbappleoff` (property `utb-apple-off`): Disable existing `ports` and `port-count` removal

* `-utbmapoff` (property `utb-map-off`): Disable custom map (useful for testing)

* `utbwait=XXX` (property `utb-wait`, type number): Custom delay for `waitForMatchingService`, in seconds. Integer between 1-180, inclusive.

## Converting Existing Maps

Converting existing maps is fairly easy.

* For each IOKit personality, change the following:
  * `CFBundleIdentifier` to `com.dhinakg.USBToolBox.kext`
  * `IOClass` to `USBToolBox`
  * `IOMatchCategory` to `USBToolBox`
* Add a dictionary named `OSBundleLibraries` to the root item. It should contain `com.dhinakg.USBToolBox.kext`, with value `1.0.0`.

## Usage

You can get the latest release from the GitHub [releases tab](https://github.com/USBToolBox/kext/releases).

The zip contains 2 kexts: the main `USBToolBox.kext`, and `UTBDefault.kext`, a codeless kext used for attaching USBToolBox to all PCIe USB controllers. This is designed for use before you map, so that you can have all USB ports working (assuming no port limit) before you map. However, it is not needed if you choose to map from the start (ie. from Windows, using the USBToolBox [tool](https://github.com/USBToolBox/tool)).

A basic fresh install flow would be as follows:

1. Add `USBToolBox.kext` and `UTBDefault.kext` to your `EFI/OC/Kexts` folder, and make sure to update your `config.plist`.
2. Install macOS.
3. Map your ports with the USBToolBox [tool](https://github.com/USBToolBox/tool).
4. Remove `UTBDefault.kext` and add your newly created `UTBMap.kext` (or whatever your USB map is called) to `EFI/OC/Kexts`.
5. Reboot and you should have a USB mapped system!

## Credits

@RehabMan for [USBInjectAll](https://github.com/RehabMan/USBInjectAll), an inspiration for this project

@acidanthera for [MacKernelSDK](https://github.com/acidanthera/MacKernelSDK)

My testing team (you know who you are) for testing
