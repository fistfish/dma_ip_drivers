# XDMA Windows Driver Build Instructions

## Prerequisites

1. Visual Studio 2019 or later with the following components:
   - Windows SDK 10.0.19041.0 or later
   - MSVC v142 build tools or later
   - Windows Universal CRT SDK

2. Windows Driver Kit (WDK) 10.0.19041.0 or later
   - Download from [Microsoft WDK](https://docs.microsoft.com/en-us/windows-hardware/drivers/download-the-wdk)
   - Install Visual Studio Integration during WDK installation

## Build Instructions

1. Open Visual Studio Developer Command Prompt
2. Navigate to the solution directory:
   ```cmd
   cd XDMA\win-kmdf
   ```
3. Build the driver:
   ```cmd
   msbuild xdma_win.sln /p:Configuration=Release /p:Platform=x64
   ```

## Driver Signing

The driver must be signed for installation on Windows 10/11 64-bit systems.

### Test Signing (Development)
1. Create a test certificate:
   ```cmd
   makecert -r -pe -ss PrivateCertStore -n CN=XdmaTestCert xdma_test.cer
   ```
2. Sign the driver:
   ```cmd
   signtool sign /v /s PrivateCertStore /n XdmaTestCert /t http://timestamp.digicert.com x64/Release/xdma.sys
   ```
3. Enable test signing on target machine:
   ```cmd
   bcdedit /set testsigning on
   ```

### Production Signing
For production deployment, the driver must be signed using:
- Windows Hardware Compatibility Program (WHCP) submission
- EV Code Signing Certificate
- Hardware Dev Center Dashboard

## Installation

1. Copy the following files to the target system:
   - `x64/Release/xdma.sys` (Driver binary)
   - `x64/Release/xdma.inf` (Driver information file)
   - `x64/Release/xdma.cat` (Security catalog)

2. Install using Device Manager:
   - Update driver for the Xilinx XDMA device
   - Browse to the folder containing the driver files
   - Select "Install this driver software anyway" if prompted

## Troubleshooting

1. Verify WDK installation:
   ```cmd
   where msbuild
   where signtool
   ```

2. Common build errors:
   - Missing WDK: Install WDK with Visual Studio integration
   - Platform SDK not found: Install correct Windows SDK version
   - Signing failed: Verify certificate and timestamp server availability

3. Installation issues:
   - Driver not loading: Check Event Viewer for details
   - Code signing error: Verify test signing is enabled for development
   - Device not found: Verify hardware presence and PCIe enumeration

## Support

For issues related to the XDMA driver, please use:
- Xilinx PCIe Forum for hardware-specific questions
- GitHub Issues for build-related problems

## Version Information

The driver version matches the Linux driver:
- Major: 2020
- Minor: 2
- Patch: 3

## Performance Considerations

- Enable/disable debug prints using the 'Debug Print Filter' in registry
- Configure DMA buffer sizes based on your application requirements
- Monitor performance using Windows Performance Analyzer (WPA)
