# Driver for SPL2 and SPLc laser printers from Samsung, Xerox, Dell, Lexmark, and Toshiba

Support for printing to SPL2- and SPLc-based printers. These are most of the cheaper laser printers from Samsung, but also rebranded ones from Xerox, Dell, Lexmark, and Toshiba. This driver is especially for those models not understanding standard languages like PostScript or PCL.

Both monochrome (ML-15xx, ML-16xx, ML-17xx, ML-18xx, ML-2xxx) and color (CLP-5xx, CLP-6xx) models are supported, and also their rebranded equivalents like the Xerox Phaser 6100 work with this driver.

Note that older SPL1-based models (ML-12xx, ML-14xx) do not work. Use these printers with the older "gdi" driver which is built into GhostScript.

See installation instructions in the INSTALL file.

The driver was created by Aurélien Croc and contains many contributions from Till Kamppeter. In 2024-2025, the project was revitalized and modernized:
- **C++23 Modernization**: Full refactor of core data structures using modern RAII, member initializers, and standard library components (e.g., `std::span`, `std::semaphore`).
- **CMake Build System**: Complete replacement of the legacy Makefile system with a robust, cross-platform CMake configuration supporting presets and multi-architecture builds.
- **Robustness**: Integrated AddressSanitizer (ASan) and GTest-based unit testing for critical compression and synchronization code.
- **CI/CD**: Fully automated GitHub Actions pipeline for verified releases.
