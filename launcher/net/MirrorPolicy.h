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

#include <QString>
#include <QUrl>
#include <vector>

namespace Net {

/**
 * How to order the official host against mirrors.
 *
 * OfficialOnly keeps the upstream behaviour. The mirror is not faster everywhere - measured from
 * a host in Japan it was 5 to 15 times slower than Mojang and occasionally stalled for tens of
 * seconds - so nothing may assume a mirror is the better default.
 */
enum class MirrorPolicyKind : int {
    OfficialOnly = 0,
    OfficialThenMirror = 1,
    MirrorThenOfficial = 2,
    MirrorOnly = 3,
};

/**
 * Pure URL rewriting for download mirrors.
 *
 * Mirrors of Mojang content are addressed by swapping the host and, for some hosts, prefixing a
 * path segment; the rest of the URL is untouched. Keeping this free of settings and network
 * access is what makes it testable.
 */
class MirrorPolicy {
public:
    /** Mirrors of `url`, in the order they should be tried; empty when the host is not mirrored. */
    static auto mirrorUrls(const QUrl& url, const QString& mirrorBase) -> std::vector<QUrl>;

    /** The full attempt order for a given policy. Always contains at least `url`. */
    static auto candidatesFor(const QUrl& url, MirrorPolicyKind kind, const QString& mirrorBase) -> std::vector<QUrl>;

    /** Same, using the user's settings. Returns just `url` when there is no launcher instance. */
    static auto candidates(const QUrl& url) -> std::vector<QUrl>;
};

/**
 * The ordered sources one request may try.
 *
 * Kept separate from Request so the ordering and the exhaustion rule are testable without a
 * running application or a network.
 */
class SourceSequence {
public:
    SourceSequence() = default;
    explicit SourceSequence(std::vector<QUrl> candidates) : m_urls(std::move(candidates)) {}

    auto isEmpty() const -> bool { return m_urls.empty(); }
    auto current() const -> QUrl { return m_urls.at(m_index); }
    auto remaining() const -> size_t { return m_urls.size() - m_index - 1; }

    /** Moves to the next source. Returns false when there is none left, leaving the index alone. */
    auto advance() -> bool
    {
        if (m_index + 1 >= m_urls.size())
            return false;
        ++m_index;
        return true;
    }

    [[nodiscard]] const std::vector<QUrl>& urls() const { return m_urls; }

private:
    std::vector<QUrl> m_urls;
    size_t m_index = 0;
};

}  // namespace Net
