# cmake/SpliXCPack.cmake

# CPack configuration for building a Debian package
set(CPACK_GENERATOR "DEB")
set(CPACK_DEBIAN_PACKAGE_DEPENDS "cups, libcups2, libcupsimage2, libjbig0")

# Architecture mapping
if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|amd64)$")
    set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE "amd64")
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "^(aarch64|arm64)$")
    set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE "arm64")
else()
    set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE "${CMAKE_SYSTEM_PROCESSOR}")
endif()

# Metadata
set(CPACK_PACKAGE_NAME "splix")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_DEBIAN_PACKAGE_SECTION "utils")
set(CPACK_DEBIAN_PACKAGE_PRIORITY "optional")
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "github-actions@noreply.github.com")
set(CPACK_DEBIAN_PACKAGE_DESCRIPTION "SpliX printer drivers for Samsung and Xerox printers")

set(CPACK_PACKAGE_FILE_NAME "splix-ubuntu-${CPACK_PACKAGE_VERSION}-${CPACK_DEBIAN_PACKAGE_ARCHITECTURE}")

include(CPack)
