// SPDX-License-Identifier: GPL-3.0-only
#include "komorebi/app/LaunchProbe.h"

#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QProcess>
#include <QTextStream>

#include "komorebi/core/launch/LaunchPlan.h"
#include "komorebi/core/version/VersionChain.h"
#include "komorebi/services/java/JavaRuntime.h"

namespace Komorebi::App {

namespace {

QString valueOf(const QStringList& arguments, const QString& name, const QString& fallback = {})
{
    const int index = arguments.indexOf(name);
    if (index >= 0 && index + 1 < arguments.size())
        return arguments.at(index + 1);
    return fallback;
}

void say(QTextStream& out, const QString& line)
{
    out << line << Qt::endl;
}

}  // namespace

int probeLaunch(const QStringList& arguments)
{
    QTextStream out(stdout);

    const QString dataRoot = valueOf(arguments, QStringLiteral("--komorebi-data"));
    const QString versionId = valueOf(arguments, QStringLiteral("--komorebi-version"));
    if (dataRoot.isEmpty() || versionId.isEmpty()) {
        say(out, QStringLiteral("probe: --komorebi-data and --komorebi-version are required"));
        return 2;
    }

    const QDir data(dataRoot);
    const QString versionsRoot = data.filePath(QStringLiteral("versions"));
    const QString gameDirectory = valueOf(arguments, QStringLiteral("--komorebi-game-dir"),
                                          QDir(versionsRoot).filePath(versionId));
    const QString username = valueOf(arguments, QStringLiteral("--komorebi-username"), QStringLiteral("Player"));
    const int timeoutSeconds = valueOf(arguments, QStringLiteral("--komorebi-timeout"), QStringLiteral("0")).toInt();

    QString error;
    const auto chain = Core::loadVersionChain(versionsRoot, versionId, &error);
    if (!chain) {
        say(out, QStringLiteral("probe: %1").arg(error));
        return 2;
    }
    const auto merged = Core::mergeChain(*chain, &error);
    if (!merged) {
        say(out, QStringLiteral("probe: %1").arg(error));
        return 2;
    }

    const int requiredJava = merged->javaMajor > 0 ? merged->javaMajor : 8;
    const auto runtimes = Services::discover(dataRoot);
    const auto java = Services::choose(runtimes, requiredJava);
    if (!java) {
        say(out, QStringLiteral("probe: no Java %1 found; seen: %2")
                     .arg(requiredJava)
                     .arg([&runtimes] {
                         QStringList described;
                         for (const auto& runtime : runtimes)
                             described << QStringLiteral("%1 (%2)").arg(runtime.path).arg(runtime.version);
                         return described.join(QStringLiteral(", "));
                     }()));
        return 2;
    }

    const QString natives = QDir(gameDirectory).filePath(QStringLiteral("natives"));
    QDir().mkpath(natives);

    Core::LaunchOptions options;
    options.javaPath = java->path;
    options.gameDirectory = gameDirectory;
    options.assetsDirectory = data.filePath(QStringLiteral("assets"));
    options.librariesDirectory = data.filePath(QStringLiteral("libraries"));
    options.nativesDirectory = natives;
    options.clientJarPath = Core::clientJarPath(QDir(versionsRoot).filePath(versionId), versionId);
    options.versionName = versionId;
    options.username = username;
    options.uuid = QStringLiteral("00000000000000000000000000000000");
    options.accessToken = QStringLiteral("0");

    const auto plan = Core::buildLaunchPlan(*merged, options);
    if (!plan.ok()) {
        say(out, QStringLiteral("probe: the command line is incomplete: %1").arg(plan.problems.join(QStringLiteral("; "))));
        return 2;
    }

    say(out, QStringLiteral("probe: java %1 (%2)").arg(java->path, java->version));
    say(out, QStringLiteral("probe: version %1 (%2 libraries, main %3)")
                 .arg(merged->id)
                 .arg(merged->libraries.size())
                 .arg(merged->mainClass));
    say(out, QStringLiteral("probe: command %1 %2").arg(plan.program, plan.arguments.join(QLatin1Char(' '))));

    QProcess game;
    game.setProgram(plan.program);
    game.setArguments(plan.arguments);
    game.setWorkingDirectory(plan.workingDirectory);
    game.setProcessChannelMode(QProcess::MergedChannels);
    game.start();
    if (!game.waitForStarted(10000)) {
        say(out, QStringLiteral("probe: java did not start: %1").arg(game.errorString()));
        return 3;
    }

    QElapsedTimer elapsed;
    elapsed.start();
    while (game.state() != QProcess::NotRunning) {
        if (game.waitForReadyRead(500)) {
            const QString chunk = QString::fromLocal8Bit(game.readAll());
            out << chunk;
            out.flush();
        }
        if (timeoutSeconds > 0 && elapsed.elapsed() > timeoutSeconds * 1000) {
            say(out, QStringLiteral("probe: stopping the game after %1 seconds").arg(timeoutSeconds));
            game.kill();
            game.waitForFinished(5000);
            return 0;
        }
    }

    out << QString::fromLocal8Bit(game.readAll());
    out.flush();
    const int exitCode = game.exitCode();
    say(out, QStringLiteral("probe: the game exited with %1").arg(exitCode));
    return exitCode;
}

}  // namespace Komorebi::App
