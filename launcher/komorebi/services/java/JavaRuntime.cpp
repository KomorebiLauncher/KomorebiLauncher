// SPDX-License-Identifier: GPL-3.0-only
#include "komorebi/services/java/JavaRuntime.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QStandardPaths>
#include <algorithm>

namespace Komorebi::Services {

namespace {

/// The value of a `release` line, which is KEY="value" with the quotes optional.
QString releaseValue(const QString& line)
{
    const int equals = line.indexOf(QLatin1Char('='));
    if (equals < 0)
        return {};
    QString value = line.mid(equals + 1).trimmed();
    if (value.startsWith(QLatin1Char('"')) && value.endsWith(QLatin1Char('"')) && value.size() >= 2)
        value = value.mid(1, value.size() - 2);
    return value;
}

/// Reads a runtime from a home directory, if it looks like one.
std::optional<JavaRuntime> runtimeAt(const QString& home)
{
    const QString executable = executableIn(home);
    if (executable.isEmpty() || !QFileInfo::exists(executable))
        return std::nullopt;

    QFile release(QDir(home).filePath(QStringLiteral("release")));
    if (release.open(QIODevice::ReadOnly | QIODevice::Text)) {
        const QByteArray text = release.readAll();
        if (auto runtime = parseRelease(QString::fromUtf8(text), home, executable))
            return runtime;
    }

    // No release file: the directory's name often carries the version ("jdk-17.0.2").
    JavaRuntime runtime;
    runtime.home = home;
    runtime.path = executable;
    runtime.major = majorFromVersion(QFileInfo(home).fileName());
    if (!runtime.isValid())
        return std::nullopt;
    return runtime;
}

void appendRuntimesFrom(QList<JavaRuntime>* runtimes, const QString& root)
{
    if (root.isEmpty())
        return;

    // Either this is a runtime home itself, or it holds one per version.
    if (auto runtime = runtimeAt(root)) {
        runtimes->append(*runtime);
        return;
    }
    const QDir directory(root);
    for (const auto& entry : directory.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name))
        if (auto runtime = runtimeAt(entry.absoluteFilePath()))
            runtimes->append(*runtime);
}

}  // namespace

QString executableIn(const QString& home)
{
    if (home.isEmpty())
        return {};
    return QDir(home).filePath(QStringLiteral("bin/%1").arg(
#if defined(Q_OS_WIN)
        QStringLiteral("java.exe")
#else
        QStringLiteral("java")
#endif
        ));
}

int majorFromVersion(const QString& version)
{
    const auto text = version.trimmed();
    if (text.isEmpty())
        return 0;

    static const QRegularExpression legacy(QStringLiteral("^1\\.(\\d+)"));
    if (const auto match = legacy.match(text); match.hasMatch())
        return match.captured(1).toInt();

    static const QRegularExpression modern(QStringLiteral("^(\\d+)"));
    if (const auto match = modern.match(text); match.hasMatch())
        return match.captured(1).toInt();

    // A directory named "jdk-17.0.2" or "java-21-openjdk" still names a version.
    static const QRegularExpression anywhere(QStringLiteral("(\\d+)(?:\\.|$|_)"));
    if (const auto match = anywhere.match(text); match.hasMatch())
        return match.captured(1).toInt();

    return 0;
}

std::optional<JavaRuntime> parseRelease(const QString& text, const QString& home, const QString& executable)
{
    if (text.isEmpty() || home.isEmpty())
        return std::nullopt;

    JavaRuntime runtime;
    runtime.home = home;
    runtime.path = executable.isEmpty() ? executableIn(home) : executable;

    for (const auto& rawLine : text.split(QLatin1Char('\n'))) {
        const QString line = rawLine.trimmed();
        if (line.startsWith(QLatin1String("JAVA_VERSION"))) {
            runtime.version = releaseValue(line);
        } else if (line.startsWith(QLatin1String("OS_ARCH"))) {
            runtime.architecture = releaseValue(line);
        } else if (line.startsWith(QLatin1String("IMPLEMENTOR"))) {
            runtime.vendor = releaseValue(line);
        }
    }

    runtime.major = majorFromVersion(runtime.version);
    if (!runtime.isValid())
        return std::nullopt;
    return runtime;
}

QList<JavaRuntime> scanMojangRuntimes(const QString& dataRoot)
{
    QList<JavaRuntime> runtimes;
    if (dataRoot.isEmpty())
        return runtimes;

    // java/<component>/<arch>/<component>/ -- the layout the launcher downloads runtimes into.
    const QDir javaRoot(QDir(dataRoot).filePath(QStringLiteral("java")));
    for (const auto& component : javaRoot.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name)) {
        for (const auto& arch : QDir(component.absoluteFilePath()).entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name)) {
            const QString nested = QDir(arch.absoluteFilePath()).filePath(component.fileName());
            if (auto runtime = runtimeAt(nested)) {
                runtimes.append(*runtime);
                continue;
            }
            if (auto runtime = runtimeAt(arch.absoluteFilePath()))
                runtimes.append(*runtime);
        }
    }
    return runtimes;
}

QList<JavaRuntime> scanInstalledRuntimes()
{
    QList<JavaRuntime> runtimes;

    appendRuntimesFrom(&runtimes, qEnvironmentVariable("JAVA_HOME"));

    QStringList roots;
#if defined(Q_OS_WIN)
    const QString programFiles = qEnvironmentVariable("ProgramFiles");
    const QString programFilesX86 = qEnvironmentVariable("ProgramFiles(x86)");
    const QString localAppData = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
    const QString home = QDir::homePath();
    roots << QDir(programFiles).filePath(QStringLiteral("Java"));
    roots << QDir(programFiles).filePath(QStringLiteral("Eclipse Adoptium"));
    roots << QDir(programFiles).filePath(QStringLiteral("Microsoft"));
    roots << QDir(programFilesX86).filePath(QStringLiteral("Java"));
    roots << QDir(home).filePath(QStringLiteral(".jdks"));
    // The launcher's own download target, when it is not the data directory.
    roots << QDir(localAppData).filePath(QStringLiteral("Programs/Eclipse Adoptium"));
#elif defined(Q_OS_MACOS)
    roots << QStringLiteral("/Library/Java/JavaVirtualMachines");
    roots << QDir(QDir::homePath()).filePath(QStringLiteral("Library/Java/JavaVirtualMachines"));
#else
    roots << QStringLiteral("/usr/lib/jvm");
    roots << QStringLiteral("/usr/java");
#endif

    for (const auto& root : roots)
        appendRuntimesFrom(&runtimes, root);

    return runtimes;
}

QList<JavaRuntime> discover(const QString& dataRoot)
{
    QList<JavaRuntime> runtimes;
    const auto append = [&runtimes](const QList<JavaRuntime>& found) {
        for (const auto& runtime : found) {
            const bool known = std::any_of(runtimes.cbegin(), runtimes.cend(),
                                           [&runtime](const JavaRuntime& other) { return other.path == runtime.path; });
            if (!known)
                runtimes.append(runtime);
        }
    };

    append(scanMojangRuntimes(dataRoot));
    append(scanInstalledRuntimes());
    return runtimes;
}

std::optional<JavaRuntime> choose(const QList<JavaRuntime>& runtimes, int requiredMajor)
{
    if (requiredMajor <= 0)
        return std::nullopt;

    const JavaRuntime* exact = nullptr;
    const JavaRuntime* above = nullptr;
    const JavaRuntime* below = nullptr;

    for (const auto& runtime : runtimes) {
        if (!runtime.isValid())
            continue;
        if (runtime.major == requiredMajor) {
            if (!exact || runtime.version > exact->version)
                exact = &runtime;
        } else if (runtime.major > requiredMajor) {
            if (!above || runtime.major < above->major)
                above = &runtime;
        } else {
            if (!below || runtime.major > below->major)
                below = &runtime;
        }
    }

    if (exact)
        return *exact;
    if (above)
        return *above;
    if (below)
        return *below;
    return std::nullopt;
}

}  // namespace Komorebi::Services
