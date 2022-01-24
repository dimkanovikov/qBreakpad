/*
 *  Copyright (C) 2009 Aleksey Palazhchenko
 *  Copyright (C) 2014 Sergey Shambir
 *  Copyright (C) 2016 Alexander Makarov
 *  Copyright (C) 2022 Dimka Novikov
 *
 * This file is a part of Breakpad-qt library.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 */

#include "QBreakpadHandler.h"

#if defined(Q_OS_MAC)
#include "client/mac/handler/exception_handler.h"
#elif defined(Q_OS_LINUX)
#include "client/linux/handler/exception_handler.h"
#elif defined(Q_OS_WIN32)
#include "client/windows/handler/exception_handler.h"
#endif

#include <QDir>

namespace {

const QLatin1String kPathSeparator("/");
static QString s_logDirPath;
static QString s_logFilePath;

#if defined(Q_OS_WIN32)
bool minidumpHandlerCallback(const wchar_t* dump_dir, const wchar_t* minidump_id, void* context,
                             EXCEPTION_POINTERS* exinfo, MDRawAssertionInfo* assertion,
                             bool succeeded)
#elif defined(Q_OS_MAC)
bool minidumpHandlerCallback(const char* dump_dir, const char* minidump_id, void* context,
                             bool succeeded)
#else
bool minidumpHandlerCallback(const google_breakpad::MinidumpDescriptor& descriptor, void* context,
                             bool succeeded)
#endif
{
#ifdef Q_OS_LINUX
    Q_UNUSED(descriptor);
#endif
    Q_UNUSED(context);
#if defined(Q_OS_WIN32)
    Q_UNUSED(assertion);
    Q_UNUSED(exinfo);
#endif

    /*
        NO STACK USE, NO HEAP USE THERE !!!
        Creating QString's, using qDebug, etc. - everything is crash-unfriendly.
    */

    //
    // Логируем информацию о сохранённом краше
    //
    QString path;
#if defined(Q_OS_WIN32)
    path
        = QString::fromWCharArray(dump_dir) + kPathSeparator + QString::fromWCharArray(minidump_id);
#elif defined(Q_OS_MAC)
    path = QString::fromUtf8(dump_dir) + kPathSeparator + QString::fromUtf8(minidump_id);
#else
    path = descriptor.path();
#endif
    qDebug("%s, dump path: %s\n",
           succeeded ? "Succeed to write minidump" : "Failed to write minidump", qPrintable(path));

    //
    // Создаём копию файла лога с именем как у дампа
    //
    QFile::copy(s_logFilePath,
                s_logDirPath + path.split(kPathSeparator).last() + QLatin1String(".log"));

    return succeeded;
}

} // namespace


void QBreakpadHandler::init(const QString& _dumpPath, const QString& _logFilePath)
{
    Q_ASSERT(QDir::isAbsolutePath(_dumpPath));
    QDir().mkpath(_dumpPath);
    if (!QDir().exists(_dumpPath)) {
        qDebug("Failed to set dump path which not exists: %s", qPrintable(_dumpPath));
        return;
    }

    Q_ASSERT(!_logFilePath.isEmpty());
    s_logDirPath = QFileInfo(_logFilePath).absolutePath() + kPathSeparator;
    s_logFilePath = _logFilePath;

    //
    // Инициилизируем обработчик исключений
    //
#if defined(Q_OS_WIN32)
    new google_breakpad::ExceptionHandler(_dumpPath.toStdWString(), /*FilterCallback*/ 0,
                                          minidumpHandlerCallback, /*context*/ 0,
                                          google_breakpad::ExceptionHandler::HANDLER_ALL);
#elif defined(Q_OS_MAC)
    new google_breakpad::ExceptionHandler(_dumpPath.toStdString(),
                                          /*FilterCallback*/ 0, minidumpHandlerCallback,
                                          /*context*/ 0, true, NULL);
#else
    new google_breakpad::ExceptionHandler(
        google_breakpad::MinidumpDescriptor(_dumpPath.toStdString()),
        /*FilterCallback*/ 0, minidumpHandlerCallback,
        /*context*/ 0, true, -1);
#endif
}
