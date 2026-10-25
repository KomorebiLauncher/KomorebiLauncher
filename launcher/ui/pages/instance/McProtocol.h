// SPDX-License-Identifier: GPL-3.0-only
/*
 *  WTF Launcher - Minecraft Launcher
 *  Copyright (C) 2026 WTFLauncher Contributors
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, version 3.
 */

#pragma once

#include <QByteArray>
#include <QString>

/**
 * The encoding half of the Minecraft status protocol.
 *
 * Split out of McClient because the handshake is the one place where a wrong bit still produces a
 * well-formed packet: the server reads a plausible but different number and answers, or does not,
 * with no way to tell from the outside which happened.
 */
namespace McProtocol {

// From https://wiki.vg/Protocol#VarInt_and_VarLong
constexpr auto varIntValueMask = 0x7F;
constexpr auto varIntContinueBit = 0x80;

/** Writes a VarInt; the value is encoded as two's complement, so -1 takes the full five bytes. */
void writeVarInt(QByteArray& data, int value);

/** Writes a length-prefixed UTF-8 string, the prefix counting bytes rather than UTF-16 code units. */
void writeString(QByteArray& data, const QString& value);

}  // namespace McProtocol
