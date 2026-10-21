// SPDX-License-Identifier: GPL-3.0-only
/*
 *  WTF Launcher - Minecraft Launcher
 *  Copyright (C) 2026 WTFLauncher Contributors
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, version 3.
 */

#include "JavaSelection.h"

#include <QRegularExpression>
#include <QSet>

namespace Java {

namespace {
// Guards against "1-999999" turning into a million iterations.
constexpr int kMaxRangeSpan = 64;
}  // namespace

auto parseVersionList(const QString& spec) -> QList<int>
{
    QSet<int> majors;
    static const QRegularExpression s_range(QStringLiteral("^\\s*(\\d+)\\s*[-~]\\s*(\\d+)\\s*$"));
    static const QRegularExpression s_single(QStringLiteral("^\\s*(\\d+)\\s*$"));

    for (const auto& rawPart : spec.split(QLatin1Char(','), Qt::SkipEmptyParts)) {
        const auto part = rawPart.trimmed();
        if (auto match = s_range.match(part); match.hasMatch()) {
            int from = match.captured(1).toInt();
            int to = match.captured(2).toInt();
            if (to < from)
                std::swap(from, to);
            if (to - from > kMaxRangeSpan)
                to = from + kMaxRangeSpan;
            for (int major = from; major <= to; ++major)
                majors.insert(major);
        } else if (auto single = s_single.match(part); single.hasMatch()) {
            majors.insert(single.captured(1).toInt());
        }
        // Anything else is ignored: an unparseable entry should not disable the whole filter.
    }

    auto result = QList<int>(majors.begin(), majors.end());
    std::sort(result.begin(), result.end());
    return result;
}

auto isVersionAllowed(int major, const QList<int>& allowed) -> bool
{
    return allowed.isEmpty() || allowed.contains(major);
}

auto pickInstall(const QList<JavaInstallPtr>& installs, const QList<int>& requiredMajors, const QList<int>& allowed) -> JavaInstallPtr
{
    for (const auto& install : installs) {
        if (!install)
            continue;
        if (!requiredMajors.contains(install->id.major()))
            continue;
        if (!isVersionAllowed(install->id.major(), allowed))
            continue;
        return install;
    }
    return nullptr;
}

}  // namespace Java
