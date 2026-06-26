//#include "mainwindow.h"

#include <QApplication>
#include <QSystemTrayIcon>
#include <QMenu>

#include <QDebug>
#include <QFileSystemWatcher>
#include <QFile>

#include <QDir>
#include <QFileInfo>
#include <QDebug>

#include <QTimer>
#include <QMap>

#include <QSettings>
#include <QDateTime>
#include <QCoreApplication>

struct SyncPair {
    QString sourceDir;
    QString destinationDir;
};

struct AppConfig {
    int pollIntervalSeconds = 60;
    int debounceSeconds = 3;
    bool loggingEnabled = false;
    QString logFilePath;
};

QList<SyncPair> syncList;
QMap<QString, QTimer*> debounceTimers;
AppConfig g_config;

// ---------------------------------------------------------------------------
// Optional file logging
// ---------------------------------------------------------------------------
static QFile g_logFile;
static QtMessageHandler g_defaultHandler = nullptr;

void messageHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
    // Keep default behaviour (debugger / console output) as well.
    if (g_defaultHandler)
        g_defaultHandler(type, context, msg);

    if (!g_logFile.isOpen())
        return;

    const char *level = "INFO";
    switch (type) {
    case QtDebugMsg:    level = "DEBUG"; break;
    case QtInfoMsg:     level = "INFO";  break;
    case QtWarningMsg:  level = "WARN";  break;
    case QtCriticalMsg: level = "ERROR"; break;
    case QtFatalMsg:    level = "FATAL"; break;
    }

    const QString line = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss.zzz")
                         + " [" + level + "] " + msg + "\n";
    g_logFile.write(line.toUtf8()); // toUtf8 keeps emoji/non-ASCII intact on all Qt versions
    g_logFile.flush();
}

// Resolve a possibly-relative path against the application directory.
static QString resolveAgainstAppDir(const QString &path)
{
    if (path.isEmpty() || QDir::isAbsolutePath(path))
        return path;
    return QDir(QCoreApplication::applicationDirPath()).filePath(path);
}

// Write a default config the first time the app runs, preserving the original
// hardcoded behaviour so existing setups keep working.
static void writeDefaultConfig(const QString &configPath)
{
    QSettings settings(configPath, QSettings::IniFormat);

    settings.setValue("General/pollIntervalSeconds", 60);
    settings.setValue("General/debounceSeconds", 3);

    settings.setValue("Logging/enabled", false);
    settings.setValue("Logging/file", "VideoSync.log");

    settings.beginGroup("Movies");
    settings.setValue("source", R"(D:\Torrents\Video\Movies)");
    settings.setValue("destination", R"(\\Nas\data\Video\Movies)");
    settings.endGroup();

    settings.beginGroup("TVShows");
    settings.setValue("source", R"(D:\Torrents\Video\TVShows)");
    settings.setValue("destination", R"(\\Nas\data\Video\TVShows)");
    settings.endGroup();

    settings.sync();
    qDebug() << "Created default config at" << configPath;
}

// Load configuration from VideoSync.ini next to the executable, creating a
// default one if it does not exist yet. Populates the global syncList.
static void loadConfig()
{
    const QString configPath =
        QDir(QCoreApplication::applicationDirPath()).filePath("VideoSync.ini");

    if (!QFile::exists(configPath))
        writeDefaultConfig(configPath);

    QSettings settings(configPath, QSettings::IniFormat);

    g_config.pollIntervalSeconds = settings.value("General/pollIntervalSeconds", 60).toInt();
    g_config.debounceSeconds     = settings.value("General/debounceSeconds", 3).toInt();
    g_config.loggingEnabled      = settings.value("Logging/enabled", false).toBool();
    g_config.logFilePath = resolveAgainstAppDir(
        settings.value("Logging/file", "VideoSync.log").toString());

    // Every group that is not General/Logging is treated as a sync pair.
    syncList.clear();
    const QStringList groups = settings.childGroups();
    for (const QString &group : groups) {
        if (group.compare("General", Qt::CaseInsensitive) == 0 ||
            group.compare("Logging", Qt::CaseInsensitive) == 0)
            continue;

        settings.beginGroup(group);
        const QString source = settings.value("source").toString();
        const QString dest   = settings.value("destination").toString();
        settings.endGroup();

        if (source.isEmpty() || dest.isEmpty()) {
            qWarning() << "Skipping incomplete sync pair" << group << "in config";
            continue;
        }
        syncList.append({ source, dest });
    }

    qDebug() << "Loaded" << syncList.size() << "sync pair(s) from" << configPath;
}

bool moveRecursively(const QString &srcDir, const QString &dstDir);
void performSync(QSystemTrayIcon *trayIcon, const SyncPair &pair);

void onDirectoryChanged(QSystemTrayIcon *trayIcon, const QString &changedPath) {
    qDebug() << "Detected change in:" << changedPath;

    // Find which SyncPair corresponds to this path
    SyncPair targetPair;
    bool found = false;
    for (const SyncPair &pair : std::as_const(syncList)) {
        if (QDir::toNativeSeparators(pair.sourceDir) == QDir::toNativeSeparators(changedPath)) {
            targetPair = pair;
            found = true;
            break;
        }
    }

    if (!found) {
        qDebug() << "No matching sync pair found for path:" << changedPath;
        return;
    }

    // Debounce timer per folder
    if (!debounceTimers.contains(changedPath)) {
        debounceTimers[changedPath] = new QTimer();
        debounceTimers[changedPath]->setSingleShot(true);
        QObject::connect(debounceTimers[changedPath], &QTimer::timeout, [trayIcon, targetPair]() {
            qDebug() << "Performing sync for" << targetPair.sourceDir;
            performSync(trayIcon, targetPair);
        });
    }

    // Restart timer — wait (debounceSeconds) after last change
    debounceTimers[changedPath]->start(g_config.debounceSeconds * 1000);
}

// Move file from src to dst (create folder structure as needed)
bool moveFile(const QString &srcFile, const QString &dstFile) {
    QDir dstDir = QFileInfo(dstFile).dir();
    if (!dstDir.exists()) {
        if (!dstDir.mkpath(".")) {
            qWarning() << "Failed to create destination directory:" << dstDir.path();
            return false;
        }
    }

    if (QFile::exists(dstFile)) {
        QFile::remove(dstFile); // Overwrite existing
    }
    if (!QFile::rename(srcFile, dstFile)) {
        // fallback: copy then remove
        if (QFile::copy(srcFile, dstFile)) {
            QFile::remove(srcFile);
            qDebug() << "   🎬" << srcFile << " --> " << dstFile;
            return true;
        } else {
            qWarning() << "Failed to move file:" << srcFile << "->" << dstFile;
            return false;
        }
    }
    else {
        qDebug() << "   🎬" << srcFile << " --> " << dstFile;
    }
    return true;
}

// Recursively move files (replicate directory tree)
bool moveRecursively(const QString &srcPath, const QString &dstPath) {
    QDir srcDir(srcPath);
    if (!srcDir.exists()) {
        qWarning() << "Source does not exist:" << srcPath;
        return false;
    }

    QFileInfoList entries = srcDir.entryInfoList(QDir::NoDotAndDotDot | QDir::AllEntries);
    for (const QFileInfo &entry : std::as_const(entries)) {
        QString srcFilePath = entry.absoluteFilePath();
        QString dstFilePath = dstPath + "/" + entry.fileName();

        if (entry.isDir()) {
            if (!moveRecursively(srcFilePath, dstFilePath))
                return false;
            QDir().rmdir(srcFilePath); // remove empty folder after move
        } else {
            if (!moveFile(srcFilePath, dstFilePath))
                return false;
        }
    }
    return true;
}

void performSync(QSystemTrayIcon *trayIcon, const SyncPair &pair) {
    trayIcon->setIcon(QIcon(":/icons/VideoSyncG.png"));
    QApplication::processEvents();

    qDebug() << "Moving from" << pair.sourceDir << "-->" << pair.destinationDir;
    moveRecursively(pair.sourceDir, pair.destinationDir);

    trayIcon->setIcon(QIcon(":/icons/VideoSyncB.png"));
    QApplication::processEvents();
}


int main(int argc, char *argv[])
{
    // Main app
    QCoreApplication::addLibraryPath(QCoreApplication::applicationDirPath());
    QApplication app(argc, argv);

    // Configuration (populates the global syncList)
    loadConfig();

    // Optional logging — enable only if requested in the config file.
    if (g_config.loggingEnabled && !g_config.logFilePath.isEmpty()) {
        g_logFile.setFileName(g_config.logFilePath);
        if (g_logFile.open(QIODevice::Append | QIODevice::Text)) {
            g_defaultHandler = qInstallMessageHandler(messageHandler);
            qDebug() << "===== VideoSync started, logging to" << g_config.logFilePath << "=====";
        } else {
            qWarning() << "Could not open log file:" << g_config.logFilePath;
        }
    }

    // Menu
    QMenu menu;
    QAction *syncNow = menu.addAction("Sync Now");
    QAction *quitAction = menu.addAction("Exit");

    // Notification icon
    QSystemTrayIcon *trayIcon = new QSystemTrayIcon;
    trayIcon->setIcon(QIcon(":/icons/VideoSyncB.png"));
    trayIcon->setToolTip("VideoSync is running");
    trayIcon->setContextMenu(&menu);
    trayIcon->show();

    QObject::connect(quitAction, &QAction::triggered, &app, &QCoreApplication::quit);
    // Manual "Sync Now" (user clicks in tray menu)
    QObject::connect(syncNow, &QAction::triggered, [&]() {
        for (const SyncPair &pair : syncList) {
            performSync(trayIcon, pair);
        }
    });

    // File watcher
    QFileSystemWatcher watcher;
    // Register every configured source directory to actually be watched.
    for (const SyncPair &pair : std::as_const(syncList)) {
        if (QDir(pair.sourceDir).exists()) {
            watcher.addPath(pair.sourceDir);
        } else {
            qWarning() << "Source directory does not exist, not watching:" << pair.sourceDir;
        }
    }
    // Directory change watcher
    QObject::connect(&watcher, &QFileSystemWatcher::directoryChanged, [&](const QString &path) {
        onDirectoryChanged(trayIcon, path);
    });

    // File change watcher (same logic)
    QObject::connect(&watcher, &QFileSystemWatcher::fileChanged, [&](const QString &path) {
        onDirectoryChanged(trayIcon, QFileInfo(path).absolutePath());
    });

    // Periodically recheck folders (some changes may not trigger events)
    QTimer timer;
    timer.setInterval(g_config.pollIntervalSeconds * 1000);
    QObject::connect(&timer, &QTimer::timeout, [&]() {
        for (const SyncPair &pair : syncList) {
            performSync(trayIcon, pair);
        }
    });
    timer.start();

    // MainWindow w;
    // w.show();
    return app.exec();
}
