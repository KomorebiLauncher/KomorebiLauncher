// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Komorebi Launcher - Minecraft Launcher
 *  Copyright (C) 2026 KomorebiLauncher Contributors
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, version 3.
 */

#include "McProtocol.h"

namespace McProtocol {

void writeVarInt(QByteArray& data, int value)
{
    // Seven bits at a time, least significant first, with the continuation bit set on every byte
    // but the last. The value is handled unsigned so that negative numbers (which the protocol
    // writes as five-byte two's complement) terminate instead of shifting in a sign forever.
    auto remaining = static_cast<uint32_t>(value);

    while (remaining >= varIntContinueBit) {
        data.append(static_cast<uint8_t>((remaining & varIntValueMask) | varIntContinueBit));
        remaining >>= 7;
    }
    data.append(static_cast<uint8_t>(remaining));
}

void writeString(QByteArray& data, const QString& value)
{
    // The prefix counts the bytes that go on the wire; QString::size() would count UTF-16 code
    // units, which match only for ASCII.
    const auto bytes = value.toUtf8();
    writeVarInt(data, static_cast<int>(bytes.size()));
    data.append(bytes);
}

}  // namespace McProtocol
