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
            /*
             * A development hook: start one instance through the launcher's own entry point, which now
             * goes through the core, and wait for the game so the launcher does not take it down on
             * exit.
             */
            const int coreLaunch = app.arguments().indexOf(QStringLiteral("--komorebi-launch-instance"));
            if (coreLaunch >= 0 && coreLaunch + 1 < app.arguments().size()) {
                const QString instanceId = app.arguments().at(coreLaunch + 1);
                auto* instance = APPLICATION->instances()->getInstanceById(instanceId);
                if (!instance) {
                    qInfo().noquote() << "core launch: no such instance" << instanceId;
                    return 1;
                }

                if (!APPLICATION->launch(instance)) {
                    qInfo().noquote() << "core launch: the launcher refused to start" << instanceId;
                    return 1;
                }
                qInfo().noquote() << "core launch: asked the launcher to start" << instanceId;

                // Fetching the files takes a moment, so the game's process appears later; wait for it,
                // then for the game.
                QProcess* game = nullptr;
                QTimer poll;
                poll.setInterval(500);
                QEventLoop loop;
                QObject::connect(&poll, &QTimer::timeout, &loop, [&] {
                    for (auto* process : APPLICATION->findChildren<QProcess*>()) {
                        if (!process->arguments().join(QLatin1Char(' ')).contains(instanceId))
                            continue;
                        game = process;
                        break;
                    }
                    if (game)
                        loop.quit();
                });
                poll.start();
                loop.exec();
                poll.stop();

                qInfo().noquote() << "core launch: waiting for the game, pid" << game->processId();
                game->waitForFinished(-1);
                qInfo().noquote() << "core launch: the game exited with" << game->exitCode();
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
