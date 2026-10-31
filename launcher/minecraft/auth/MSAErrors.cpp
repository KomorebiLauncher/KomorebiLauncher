// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Komorebi Launcher - Minecraft Launcher
 *  Copyright (C) 2026 Komorebi Contributors
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, version 3.
 */

#include "MSAErrors.h"

#include <QLatin1String>

namespace {

/**
 * Markers Microsoft uses when the client (our application id) is the problem.
 *
 * AADSTS700016: no application with that id exists in the directory.
 * AADSTS7000218: the application needs a client secret, i.e. it is not registered as a public client.
 * AADSTS7000215: the client secret sent is wrong (a desktop app must not need one).
 * AADSTS9002325: the redirect URI is not one the application registered.
 * invalid_client / unauthorized_client: the OAuth-level names for the same situation.
 * "invalid app registration": Mojang's own wording when it refuses the id at the Xbox stage.
 */
constexpr const char* markers[] = {
    "aadsts700016", "aadsts7000218", "aadsts7000215", "aadsts9002325",
    "invalid_client", "unauthorized_client", "invalid app registration",
};

}  // namespace

auto MSA::describesAppIdProblem(const QString& reason) -> bool
{
    const auto text = reason.toLower();
    for (const auto* marker : markers) {
        if (text.contains(QLatin1String(marker))) {
            return true;
        }
    }
    return false;
}
