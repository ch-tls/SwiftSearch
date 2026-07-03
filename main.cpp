/**
 * @file main.cpp
 * @brief SwiftSearch 应用程序入口点。
 *
 * 初始化 QApplication、日志系统，并显示主窗口。
 */

#include <QApplication>
#include "view/main_window.h"
#include "util/log_manager.h"

#include <QApplication>
#include <QTranslator>
#include <QLocale>
#include "view/main_window.h"
#include "util/log_manager.h"

int main(int argc, char* argv[]) {
  QApplication app(argc, argv);
  app.setApplicationName("SwiftSearch");
  app.setApplicationVersion("1.0.0");
  app.setOrganizationName("SwiftSearch");

  qInstallMessageHandler([](QtMsgType type, const QMessageLogContext& context, const QString& msg) {
    QString formatted = QString("%1 (%2:%3)").arg(msg, QString::fromUtf8(context.file ? context.file : ""))
                            .arg(context.line);
    switch (type) {
    case QtDebugMsg:
      SWIFT_LOG_DEBUG(QString("[Qt] %1").arg(msg));
      break;
    case QtInfoMsg:
      SWIFT_LOG_INFO(QString("[Qt] %1").arg(msg));
      break;
    case QtWarningMsg:
      SWIFT_LOG_WARNING(QString("[Qt] %1").arg(formatted));
      break;
    case QtCriticalMsg:
    case QtFatalMsg:
      SWIFT_LOG_ERROR(QString("[Qt] %1").arg(formatted));
      break;
    }
  });

  swiftsearch::LogManager::Instance().SetMinLevel(swiftsearch::LogLevel::Debug);
  SWIFT_LOG_INFO("SwiftSearch starting");

  QLocale system_locale = QLocale::system();
  SWIFT_LOG_DEBUG(QString("System locale: %1").arg(system_locale.name()));

  if (system_locale.language() == QLocale::Chinese) {
    static QTranslator translator;
    if (translator.load(":/translations/swiftsearch_zh_CN.qm")) {
      app.installTranslator(&translator);
      SWIFT_LOG_INFO("Loaded Chinese translation");
    } else {
      SWIFT_LOG_WARNING("Failed to load Chinese translation from resources");
    }
  }

  MainWindow main_window;
  main_window.show();

  int result = QApplication::exec();
  SWIFT_LOG_INFO("SwiftSearch shutting down");
  return result;
}
