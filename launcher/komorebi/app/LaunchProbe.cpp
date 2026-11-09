// SPDX-License-Identifier: GPL-3.0-only
#include "komorebi/app/LaunchProbe.h"

#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QNetworkAccessManager>
#include <QFile>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QProcess>
#include <QTextStream>
#include <QTimer>

#include "BuildConfig.h"
#include "komorebi/core/instance/InstanceFolder.h"
#include "komorebi/core/launch/LaunchPlan.h"
#include "komorebi/core/version/ComponentSet.h"
#include "komorebi/core/version/VersionChain.h"
#include "komorebi/services/auth/MicrosoftAuth.h"
#include "komorebi/services/auth/TokenVault.h"
#include "komorebi/services/download/FileDownloader.h"
#include "komorebi/services/install/InstanceCreator.h"
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

/// Starts the plan and echoes what the game says until it stops or the timeout runs out.
int runPlan(const Core::LaunchPlan& plan, int timeoutSeconds, QTextStream& out)
{
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
            out << QString::fromLocal8Bit(game.readAll());
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

/// The client jar a component manifest asks for, resolved through the libraries directory.
QString clientJarFor(const Core::VersionJson& manifest, const QString& dataRoot)
{
    if (manifest.mainJarName.isEmpty())
        return {};
    const auto relative = Core::libraryRelativePath(manifest.mainJarName);
    if (relative.isEmpty())
        return {};
    return QDir(dataRoot).filePath(QStringLiteral("libraries/") + relative);
}

/// Picks a Java runtime for the manifest and fills the options, or explains why it cannot.
bool applyJava(const Core::VersionJson& manifest, const QString& dataRoot, Core::LaunchOptions* options, QTextStream& out,
               QString* error)
{
    const int required = manifest.javaMajor > 0 ? manifest.javaMajor : 8;
    const auto runtimes = Services::discover(dataRoot);
    const auto java = Services::choose(runtimes, required);
    if (!java) {
        QStringList seen;
        for (const auto& runtime : runtimes)
            seen << QStringLiteral("%1 (%2)").arg(runtime.path, runtime.version);
        say(out, QStringLiteral("probe: no Java %1 found; seen: %2").arg(required).arg(seen.join(QStringLiteral(", "))));
        if (error)
            *error = QStringLiteral("no Java %1").arg(required);
        return false;
    }

    options->javaPath = java->path;
    say(out, QStringLiteral("probe: java %1 (%2)").arg(java->path, java->version));
    return true;
}

/// Writes the command the plan would run to a file, for comparing it against another launcher's.
void writeReport(const QStringList& arguments, const Core::LaunchPlan& plan)
{
    const QString path = valueOf(arguments, QStringLiteral("--komorebi-report"));
    if (path.isEmpty())
        return;
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly))
        return;
    file.write((plan.program + QLatin1Char(' ') + plan.arguments.join(QLatin1Char(' '))).toUtf8());
}

int probeVersion(const QStringList& arguments, QTextStream& out)
{
    const QString dataRoot = valueOf(arguments, QStringLiteral("--komorebi-data"));
    const QString versionId = valueOf(arguments, QStringLiteral("--komorebi-version"));
    const int timeoutSeconds = valueOf(arguments, QStringLiteral("--komorebi-timeout"), QStringLiteral("0")).toInt();
    if (dataRoot.isEmpty() || versionId.isEmpty()) {
        say(out, QStringLiteral("probe: --komorebi-data and --komorebi-version are required"));
        return 2;
    }

    const QDir data(dataRoot);
    const QString versionsRoot = data.filePath(QStringLiteral("versions"));
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

    Core::LaunchOptions options;
    if (!applyJava(*merged, dataRoot, &options, out, &error))
        return 2;

    const QString gameDirectory = valueOf(arguments, QStringLiteral("--komorebi-game-dir"),
                                          QDir(versionsRoot).filePath(versionId));
    QDir().mkpath(QDir(gameDirectory).filePath(QStringLiteral("natives")));
    options.gameDirectory = gameDirectory;
    options.assetsDirectory = data.filePath(QStringLiteral("assets"));
    options.librariesDirectory = data.filePath(QStringLiteral("libraries"));
    options.nativesDirectory = QDir(gameDirectory).filePath(QStringLiteral("natives"));
    options.clientJarPath = Core::clientJarPath(QDir(versionsRoot).filePath(versionId), versionId);
    options.versionName = versionId;
    options.username = valueOf(arguments, QStringLiteral("--komorebi-username"), QStringLiteral("Player"));
    options.uuid = QStringLiteral("00000000000000000000000000000000");
    options.accessToken = QStringLiteral("0");

    const auto plan = Core::buildLaunchPlan(*merged, options);
    if (!plan.ok()) {
        say(out, QStringLiteral("probe: the command line is incomplete: %1").arg(plan.problems.join(QStringLiteral("; "))));
        return 2;
    }
    say(out, QStringLiteral("probe: version %1 (%2 libraries, main %3)")
                 .arg(merged->id)
                 .arg(merged->libraries.size())
                 .arg(merged->mainClass));
    say(out, QStringLiteral("probe: command %1 %2").arg(plan.program, plan.arguments.join(QLatin1Char(' '))));
    writeReport(arguments, plan);
    return runPlan(plan, timeoutSeconds, out);
}

int probeInstance(const QStringList& arguments, QTextStream& out)
{
    const QString dataRoot = valueOf(arguments, QStringLiteral("--komorebi-data"));
    const QString instanceId = valueOf(arguments, QStringLiteral("--komorebi-probe-instance"));
    const int timeoutSeconds = valueOf(arguments, QStringLiteral("--komorebi-timeout"), QStringLiteral("0")).toInt();
    if (dataRoot.isEmpty() || instanceId.isEmpty()) {
        say(out, QStringLiteral("probe: --komorebi-data and --komorebi-probe-instance are required"));
        return 2;
    }

    const QDir data(dataRoot);
    const QString instanceDirectory = data.filePath(QStringLiteral("instances/") + instanceId);
    QString error;
    const auto folder = Core::InstanceFolder::read(instanceDirectory, &error);
    if (!folder) {
        say(out, QStringLiteral("probe: %1").arg(error));
        return 2;
    }
    say(out, QStringLiteral("probe: instance %1 (%2), game %3").arg(folder->name, folder->id, folder->minecraftVersion()));

    // An instance is a set of components, each with its own manifest: the game does not list the
    // graphics library's jars, that component does.
    const auto components = Core::loadComponents(data.filePath(QStringLiteral("meta")), folder->components, &error);
    if (!components) {
        say(out, QStringLiteral("probe: %1").arg(error));
        return 2;
    }
    const auto manifest = Core::mergeComponents(*components, &error);
    if (!manifest) {
        say(out, QStringLiteral("probe: %1").arg(error));
        return 2;
    }

    Core::LaunchOptions options;
    if (!applyJava(*manifest, dataRoot, &options, out, &error))
        return 2;

    const QString natives = QDir(instanceDirectory).filePath(QStringLiteral("natives"));
    QDir().mkpath(natives);
    options.gameDirectory = instanceDirectory;
    options.assetsDirectory = data.filePath(QStringLiteral("assets"));
    options.librariesDirectory = data.filePath(QStringLiteral("libraries"));
    options.nativesDirectory = natives;
    options.clientJarPath = clientJarFor(*manifest, dataRoot);
    options.versionName = manifest->id;
    options.username = valueOf(arguments, QStringLiteral("--komorebi-username"), QStringLiteral("Player"));
    options.uuid = QStringLiteral("00000000000000000000000000000000");
    options.accessToken = QStringLiteral("0");

    const auto plan = Core::buildLaunchPlan(*manifest, options);
    if (!plan.ok()) {
        say(out, QStringLiteral("probe: the command line is incomplete: %1").arg(plan.problems.join(QStringLiteral("; "))));
        return 2;
    }
    say(out, QStringLiteral("probe: version %1 (%2 libraries, main %3, jar %4)")
                 .arg(manifest->id)
                 .arg(manifest->libraries.size())
                 .arg(manifest->mainClass, options.clientJarPath));
    say(out, QStringLiteral("probe: command %1 %2").arg(plan.program, plan.arguments.join(QLatin1Char(' '))));
    writeReport(arguments, plan);
    return runPlan(plan, timeoutSeconds, out);
}

}  // namespace

int probeFetch(const QStringList& arguments, QTextStream& out)
{
    const int at = arguments.indexOf(QStringLiteral("--komorebi-fetch"));
    if (at < 0 || at + 2 >= arguments.size()) {
        say(out, QStringLiteral("probe: --komorebi-fetch <url> <path> is required"));
        return 2;
    }

    QNetworkAccessManager network;
    Services::FileDownloader downloader(&network);
    QEventLoop loop;
    bool ok = false;
    QString errorText;
    QObject::connect(&downloader, &Services::FileDownloader::finished, &loop, [&](bool result, const QString& reason) {
        ok = result;
        errorText = reason;
        loop.quit();
    });
    downloader.start(Services::FileDownload{ QUrl(arguments.at(at + 1)), arguments.at(at + 2), {}, -1 });
    loop.exec();
    say(out, ok ? QStringLiteral("fetch: ok") : QStringLiteral("fetch: %1").arg(errorText));
    return ok ? 0 : 1;
}

int probeAuth(const QStringList& arguments, QTextStream& out)
{
    const bool device = arguments.contains(QStringLiteral("--komorebi-auth-device"));
    const bool refresh = arguments.contains(QStringLiteral("--komorebi-auth-refresh"));
    if (!device && !refresh) {
        say(out, QStringLiteral("probe: --komorebi-auth-device or --komorebi-auth-refresh is required"));
        return 2;
    }

    const QString clientId = valueOf(arguments, QStringLiteral("--komorebi-auth-client"), BuildConfig.MSA_CLIENT_ID);
    if (clientId.isEmpty()) {
        say(out, QStringLiteral("auth: no Microsoft app id is configured"));
        return 2;
    }
    say(out, QStringLiteral("auth: client id %1").arg(clientId));

    QNetworkAccessManager network;
    Services::MicrosoftAuth auth(&network);
    auth.setClientId(clientId);

    QEventLoop loop;
    QString failure;
    QObject::connect(&auth, &Services::MicrosoftAuth::failed, &loop, [&](const QString& reason) {
        failure = reason;
        loop.quit();
    });

    if (device) {
        // The code is printed once and then polled on a timer: the browser is where the player is,
        // and blocking this process on a modal loop would make the timeout the only way out.
        int intervalMs = 5000;
        QTimer poll;
        QObject::connect(&auth, &Services::MicrosoftAuth::deviceCodeReady, &loop, [&] {
            const Services::DeviceCode code = auth.currentDeviceCode();
            say(out, QStringLiteral("auth: open %1 and enter %2 (valid for %3s)")
                         .arg(code.verificationUri, code.userCode)
                         .arg(code.expiresIn));
            intervalMs = code.interval * 1000;
            poll.start(intervalMs);
            QTimer::singleShot(code.expiresIn * 1000, &loop, [&] {
                failure = QStringLiteral("the code expired before it was approved");
                loop.quit();
            });
        });
        QObject::connect(&poll, &QTimer::timeout, &loop, [&] { auth.pollDeviceCode(auth.currentDeviceCode()); });
        QObject::connect(&auth, &Services::MicrosoftAuth::pending, &loop, [&] { say(out, QStringLiteral("auth: waiting for approval")); });
        QObject::connect(&auth, &Services::MicrosoftAuth::slowDown, &loop, [&] {
            intervalMs = qMin(intervalMs * 2, 60000);
            poll.setInterval(intervalMs);
            say(out, QStringLiteral("auth: asked to poll less often, now every %1s").arg(intervalMs / 1000));
        });
        QObject::connect(&auth, &Services::MicrosoftAuth::signedIn, &loop, [&] {
            poll.stop();
            const Services::AuthSession session = auth.session();
            say(out, QStringLiteral("auth: signed in as %1 (%2)").arg(session.username, session.uuid));
            const QString dataRoot = valueOf(arguments, QStringLiteral("--komorebi-data"));
            if (!dataRoot.isEmpty()) {
                const Services::TokenVault vault = Services::TokenVault::forDataRoot(dataRoot);
                QString error;
                if (vault.upsert(session, &error))
                    say(out, QStringLiteral("auth: saved into %1").arg(vault.path()));
                else
                    say(out, QStringLiteral("auth: could not save the account: %1").arg(error));
            }
            loop.quit();
        });
        auth.requestDeviceCode();
        loop.exec();
    } else {
        const QString store = valueOf(arguments, QStringLiteral("--komorebi-auth-store"));
        const QString dataRoot = valueOf(arguments, QStringLiteral("--komorebi-data"));
        if (store.isEmpty() && dataRoot.isEmpty()) {
            say(out, QStringLiteral("probe: --komorebi-data or --komorebi-auth-store is required"));
            return 2;
        }
        const Services::TokenVault vault = store.isEmpty() ? Services::TokenVault::forDataRoot(dataRoot) : Services::TokenVault(store);
        QString error;
        const QList<Services::AuthSession> accounts = vault.load(&error);
        if (accounts.isEmpty()) {
            say(out, QStringLiteral("auth: no accounts in %1%2")
                         .arg(vault.path(), error.isEmpty() ? QString() : QStringLiteral(": %1").arg(error)));
            return 1;
        }

        for (const auto& account : accounts) {
            say(out, QStringLiteral("auth: renewing %1 (%2)").arg(account.username, account.uuid));

            QEventLoop one;
            bool ok = false;
            QObject::connect(&auth, &Services::MicrosoftAuth::signedIn, &one, [&] {
                ok = true;
                one.quit();
            });
            QObject::connect(&auth, &Services::MicrosoftAuth::failed, &one, [&](const QString& reason) {
                failure = reason;
                one.quit();
            });
            QTimer::singleShot(120000, &one, [&] { one.quit(); });
            auth.refresh(account.refreshToken);
            one.exec();

            if (!failure.isEmpty())
                break;
            if (!ok) {
                failure = QStringLiteral("no answer while renewing %1").arg(account.username);
                break;
            }
            const Services::AuthSession session = auth.session();
            say(out, QStringLiteral("auth: %1 -> %2, %3, access token %4 characters")
                         .arg(account.username, session.username, session.uuid)
                         .arg(session.accessToken.size()));
        }
    }

    if (!failure.isEmpty()) {
        say(out, QStringLiteral("auth: %1").arg(failure));
        return 1;
    }
    return 0;
}

int probeCreate(const QStringList& arguments, QTextStream& out)
{
    const int at = arguments.indexOf(QStringLiteral("--komorebi-create-instance"));
    const QString dataRoot = valueOf(arguments, QStringLiteral("--komorebi-data"));
    if (at < 0 || at + 2 >= arguments.size() || dataRoot.isEmpty()) {
        say(out, QStringLiteral("probe: --komorebi-create-instance <version> <name> and --komorebi-data are required"));
        return 2;
    }

    QNetworkAccessManager network;
    Services::InstanceCreator creator(&network);
    QEventLoop loop;
    bool ok = false;
    QString text;
    QObject::connect(&creator, &Services::InstanceCreator::status, &loop, [&out](const QString& message) {
        say(out, QStringLiteral("create: %1").arg(message));
    });
    QObject::connect(&creator, &Services::InstanceCreator::finished, &loop, [&](const QString& directory) {
        ok = true;
        text = directory;
        loop.quit();
    });
    QObject::connect(&creator, &Services::InstanceCreator::failed, &loop, [&](const QString& reason) {
        text = reason;
        loop.quit();
    });

    Services::InstanceCreateRequest request;
    request.dataRoot = dataRoot;
    request.minecraftVersion = arguments.at(at + 1);
    request.name = arguments.at(at + 2);
    request.ensureFiles = !arguments.contains(QStringLiteral("--komorebi-create-skip-files"));
    for (const auto& argument : arguments) {
        if (argument.startsWith(QStringLiteral("--komorebi-create-loader=")))
            request.loader = argument.section(QLatin1Char('='), 1);
        else if (argument.startsWith(QStringLiteral("--komorebi-create-loader-version=")))
            request.loaderVersion = argument.section(QLatin1Char('='), 1);
        else if (argument.startsWith(QStringLiteral("--komorebi-create-forge-version=")))
            request.forgeVersion = argument.section(QLatin1Char('='), 1);
        else if (argument.startsWith(QStringLiteral("--komorebi-create-neoforge-version=")))
            request.neoforgeVersion = argument.section(QLatin1Char('='), 1);
        else if (argument.startsWith(QStringLiteral("--komorebi-create-java=")))
            request.javaPath = argument.section(QLatin1Char('='), 1);
        else if (argument == QLatin1String("--komorebi-create-mirror"))
            request.source = Services::DownloadSource::Bmclapi;
    }
    creator.start(request);
    loop.exec();

    say(out, ok ? QStringLiteral("create: made %1").arg(text) : QStringLiteral("create: %1").arg(text));
    return ok ? 0 : 1;
}
int probeLaunch(const QStringList& arguments)
{
    QTextStream out(stdout);
    if (arguments.contains(QStringLiteral("--komorebi-create-instance")))
        return probeCreate(arguments, out);
    if (arguments.contains(QStringLiteral("--komorebi-auth-device")) || arguments.contains(QStringLiteral("--komorebi-auth-refresh")))
        return probeAuth(arguments, out);
    if (arguments.contains(QStringLiteral("--komorebi-fetch")))
        return probeFetch(arguments, out);
    if (arguments.contains(QStringLiteral("--komorebi-probe-instance")))
        return probeInstance(arguments, out);
    return probeVersion(arguments, out);
}

}  // namespace Komorebi::App
