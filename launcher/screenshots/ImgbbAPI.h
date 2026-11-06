// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Komorebi Launcher - Minecraft Launcher
 *  Copyright (C) 2026 KomorebiLauncher Contributors
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, version 3.
 */

#pragma once

#include <QString>
#include <utility>

#include "Screenshot.h"
#include "net/Request.h"

/**
 * Screenshot upload to ImgBB.
 *
 * Replaces the Imgur upload this fork inherited: Imgur is unreachable from mainland China and its
 * application registration now requires a Google account, so the feature could not work for the
 * audience this project exists for. ImgBB takes the key from the player's own settings instead of
 * from the build, which keeps a credential out of the repository and lets anyone use the feature
 * with an account they registered themselves.
 */
namespace Imgbb {

/** Where the API key is kept. Empty means the player has not configured upload yet. */
inline constexpr auto kApiKeySetting = "ImgbbApiKey";

/** ImgBB's upload endpoint, without the key. */
inline constexpr auto kUploadEndpoint = "https://api.imgbb.com/1/upload";

/** True when an API key has been configured. */
auto isConfigured() -> bool;

/**
 * Builds the upload request and a pointer the caller owns the value of.
 *
 * The image is sent base64-encoded as the `image` form field, which is the only upload shape ImgBB
 * accepts for a file the launcher already has on disk.
 */
std::pair<Net::Request::Ptr, QString*> makeUpload(ScreenShot::Ptr shot);
}  // namespace Imgbb
