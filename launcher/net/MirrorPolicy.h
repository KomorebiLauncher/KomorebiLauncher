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
#include <QStringList>
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
 *
 * Two shapes of mirror exist and they are not interchangeable, which is why the maven tree gets
 * its own list. Measured on 2026-10-21, BMCLAPI serves the game files at its root, the assets
 * under /assets and the libraries under /maven, and none of those three at the others' prefix;
 * the education-network mirrors of it expose only the maven tree, at their root, with no /maven
 * segment. A single base therefore cannot stand for both kinds.
 *
 * The launcher's own services (the metadata server and the translation site) need a separate
 * base: a content mirror such as BMCLAPI does not carry them, so folding them into the same base
 * would make 'mirror only' leave the launcher unable to read any metadata at all.
 */
class MirrorPolicy {
public:
    /** Mirrors of `url`, in the order they should be tried; empty when the host is not mirrored. */
    static auto mirrorUrls(const QUrl& url, const QString& mirrorBase, const QString& servicesMirrorBase = {},
                           const QStringList& mavenMirrors = {}) -> std::vector<QUrl>;

    /** The full attempt order for a given policy. Always contains at least `url`. */
    static auto candidatesFor(const QUrl& url, MirrorPolicyKind kind, const QString& mirrorBase,
                              const QString& servicesMirrorBase = {}, const QStringList& mavenMirrors = {}) -> std::vector<QUrl>;

    /** Same, using the user's settings. Returns just `url` when there is no launcher instance. */
    static auto candidates(const QUrl& url) -> std::vector<QUrl>;
};

/**
 * Reads a mirror list stored one entry per line.
 *
 * Blank lines and lines starting with '#' are skipped, so the setting stays readable and
 * switchable by hand in the configuration file: commenting an entry out disables it without
 * losing the entry.
 */
auto parseMirrorList(const QString& text) -> QStringList;

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
    auto current() const -> QUrl { return m_index < m_urls.size() ? m_urls.at(m_index) : QUrl(); }
    auto remaining() const -> size_t { return m_index < m_urls.size() ? m_urls.size() - m_index - 1 : 0; }

    /** Moves to the next source. Returns false when there is none left, leaving the index alone. */
    auto advance() -> bool
    {
        if (m_index + 1 >= m_urls.size())
            return false;
        ++m_index;
        return true;
    }

    /** Starts over from the first source, which is what retrying the whole request needs. */
    void rewind() { m_index = 0; }

    [[nodiscard]] const std::vector<QUrl>& urls() const { return m_urls; }

private:
    std::vector<QUrl> m_urls;
    size_t m_index = 0;
};

}  // namespace Net
