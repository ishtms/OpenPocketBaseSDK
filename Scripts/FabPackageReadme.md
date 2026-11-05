# OpenPocketBase SDK 0.1.2

PocketBase integration for Unreal Engine 5.8 with Blueprint and native C++ APIs.

## Documentation and support

- Documentation: https://github.com/ishtms/OpenPocketBaseSDK
- Support: https://github.com/ishtms/OpenPocketBaseSDK/issues

## Requirements

- Unreal Engine 5.8
- Windows 64-bit or macOS
- Data Validation, an Unreal Engine plugin included with the engine and enabled automatically
- A separately hosted PocketBase server supplied and configured by the user

Networking uses Unreal's bundled libcurl and OpenSSL. Windows targets using XCurl must disable it to build this plugin. HTTP redirects are disabled; use the final PocketBase server URL.

This download contains source, with no compiled binaries. UE 5.8 builds and Mac packaged checks have not been run since the transport replacement.

## Third-party software statement

This package contains no third-party code or libraries and does not vendor, statically link, or dynamically link PocketBase software. It communicates with the user-configured PocketBase server through its public HTTP and SSE APIs using libcurl and OpenSSL supplied by Unreal Engine, plus Unreal's JSON modules. The plugin sends no data to its publisher.
