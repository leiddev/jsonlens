/*
 * version.hpp - Version of the jsonlens library.
 *
 * Copyright (c) 2026 BTXC Embedded Development Team
 * SPDX-License-Identifier: MIT
 */
#ifndef JSONLENS_VERSION_HPP
#define JSONLENS_VERSION_HPP

#define JSONLENS_VERSION_MAJOR 0
#define JSONLENS_VERSION_MINOR 1
#define JSONLENS_VERSION_PATCH 0
#define JSONLENS_VERSION_STRING "0.1.0"

namespace jsonlens {

/// Major version component.
///
/// The major component changes when the public API breaks.
inline int version_major()
{
    return JSONLENS_VERSION_MAJOR;
}

/// Minor version component.
///
/// The minor component changes when the public API grows in a compatible way.
inline int version_minor()
{
    return JSONLENS_VERSION_MINOR;
}

/// Patch version component.
///
/// The patch component changes when a released behaviour is fixed.
inline int version_patch()
{
    return JSONLENS_VERSION_PATCH;
}

/// Version of the library as "major.minor.patch".
inline const char *version_string()
{
    return JSONLENS_VERSION_STRING;
}

}  // namespace jsonlens

#endif  // JSONLENS_VERSION_HPP
