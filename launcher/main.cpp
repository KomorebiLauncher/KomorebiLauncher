// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Prism Launcher - Minecraft Launcher
 *  Copyright (C) 2022 Sefa Eyeoglu <contact@scrumplex.net>
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, version 3.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 * This file incorporates work covered by the following copyright and
 * permission notice:
 *
 *      Copyright 2013-2021 MultiMC Contributors
 *
 *      Licensed under the Apache License, Version 2.0 (the "License");
 *      you may not use this file except in compliance with the License.
 *      You may obtain a copy of the License at
 *
 *          http://www.apache.org/licenses/LICENSE-2.0
 *
 *      Unless required by applicable law or agreed to in writing, software
 *      distributed under the License is distributed on an "AS IS" BASIS,
 *      WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *      See the License for the specific language governing permissions and
 *      limitations under the License.
 */

#include <iostream>

#include <QCoreApplication>

#include "Application.h"
#include "InstanceList.h"
#include "minecraft/auth/AccountList.h"
#include "komorebi/app/LaunchProbe.h"
#include "komorebi/legacy/CoreLaunchAdapter.h"

#if defined Q_OS_WIN32
#include "console/WindowsConsole.h"
#endif

int main(int argc, char* argv[])
{
#if defined Q_OS_WIN32
    // used on Windows to attach the standard IO streams
    console::WindowsConsoleGuard _consoleGuard;
#endif

    /*
     * The development probe has to run before Application is constructed: that constructor builds the
     * window, and the point of the probe is to exercise the launch core without one. A plain
     * QCoreApplication is enough for it -- it starts a process and reads what it says.
     */
    for (int index = 1; index < argc; ++index) {
        if (qstrcmp(argv[index], "--komorebi-probe-launch") != 0 && qstrcmp(argv[index], "--komorebi-probe-instance") != 0
            && qstrcmp(argv[index], "--komorebi-fetch") != 0)
            continue;
        QCoreApplication probeApp(argc, argv);
        return Komorebi::App::probeLaunch(probeApp.arguments());
    }

    // initialize Qt
    Application app(argc, argv);
    switch (app.status()) {
        case Application::StartingUp:
        case Application::Initialized: {
            Q_INIT_RESOURCE(multimc);
            Q_INIT_RESOURCE(backgrounds);
            Q_INIT_RESOURCE(documents);
            Q_INIT_RESOURCE(komorebilauncher);

            Q_INIT_RESOURCE(pe_dark);
            Q_INIT_RESOURCE(pe_light);
            Q_INIT_RESOURCE(pe_blue);
            Q_INIT_RESOURCE(pe_colored);
            Q_INIT_RESOURCE(breeze_dark);
            Q_INIT_RESOURCE(breeze_light);
            Q_INIT_RESOURCE(flat);
            Q_INIT_RESOURCE(flat_white);

            // The Taisho icon sets, and the bundled themes: the resource objects live in a static
            // library, so nothing links them unless they are named here.
            Q_INIT_RESOURCE(taisho);
            Q_INIT_RESOURCE(taisho_dark);
            Q_INIT_RESOURCE(themes);

            Q_INIT_RESOURCE(shaders);

            /*
             * A development hook: start one instance through the core, with the account the launcher
             * already has signed in. The launcher's classes provide the account and the paths, the
             * core does the rest, and the game's output ends up in the instance's logs.
             */
            const int coreLaunch = app.arguments().indexOf(QStringLiteral("--komorebi-launch-instance"));
            if (coreLaunch >= 0 && coreLaunch + 1 < app.arguments().size()) {
                auto* instance = APPLICATION->instances()->getInstanceById(app.arguments().at(coreLaunch + 1));
                QString error;
                QString failure;
                bool started = false;
                QEventLoop loop;
                const bool accepted =
                    instance != nullptr
                    && Komorebi::Legacy::launchInstance(
                        instance, APPLICATION->accounts()->defaultAccount(),
                        [&](const QString&) {
                            started = true;
                            loop.quit();
                        },
                        [&](const QString& reason) {
                            failure = reason;
                            loop.quit();
                        },
                        &error);
                if (!accepted) {
                    qInfo().noquote() << "core launch:" << (instance ? error : QStringLiteral("no such instance"));
                    return 1;
                }
                loop.exec();
                qInfo().noquote() << "core launch:" << (started ? QStringLiteral("started") : failure);
                if (!started)
                    return 1;

                /*
                 * The game is a child process, and exiting here would destroy its QProcess and with it
                 * the game. Waiting is what a development hook wants; a windowed launch stays open and
                 * gets the same effect for free.
                 */
                const QString instanceId = app.arguments().at(coreLaunch + 1);
                for (auto* process : APPLICATION->findChildren<QProcess*>()) {
                    if (!process->arguments().join(QLatin1Char(' ')).contains(instanceId))
                        continue;
                    qInfo().noquote() << "core launch: waiting for the game, pid" << process->processId();
                    process->waitForFinished(-1);
                    qInfo().noquote() << "core launch: the game exited with" << process->exitCode();
                    break;
                }
                return 0;
            }

            return app.exec();
        }
        case Application::Failed:
            return 1;
        case Application::Succeeded:
            return 0;
        default:
            return -1;
    }
}
