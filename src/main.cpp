/*
 * Author: Copyright (C) Andrzej Surowiec 2012
 *
 * This file is part of Nitrokey.
 *
 * Nitrokey is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * Nitrokey is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Nitrokey. If not, see <http://www.gnu.org/licenses/>.
 */
#include "mainwindow.h"
//#include "mcvs-wrapper.h"
//#include "nitrokey-applet.h"
//#include "splash.h"
#include <QApplication>
#include <QTranslator>
#include <QLibraryInfo>
#include <QSettings>
#include <QFileInfo>
#include <QDebug>
//#include <QSharedMemory>
#include "src/version.h"
#include "src/utils/bool_values.h"

enum {DEBUG_STATUS_NO_DEBUGGING = 0, DEBUG_STATUS_LOCAL_DEBUG, DEBUG_STATUS_DEBUG_ALL};


bool configureParser(const QApplication &a, QCommandLineParser &parser);
void configureApplicationName();
void configureBasicTranslator(const QApplication &a, QTranslator &qtTranslator);
void issue_43_workaround();

void configureTranslator(const QApplication &a, const QCommandLineParser &parser, const QString &settings_language,
                         QTranslator &myappTranslator);

void configureRandomGenerator();

int main(int argc, char *argv[]) {
  qRegisterMetaType<QMap<QString, QVariant>>();
  issue_43_workaround();

  QApplication a(argc, argv);
  configureApplicationName();
  QCommandLineParser parser;
  auto shouldQuit = configureParser(a, parser);
  if(shouldQuit)
    return 0;

  auto debug_enabled = parser.isSet("debug");
  if(debug_enabled) {
    qDebug() << a.arguments();
  }

  // initialize i18n
//  QTranslator qtTranslator;
//  configureBasicTranslator(a, qtTranslator);


  QSettings settings;
  const auto language_key = "main/language";
  if (parser.isSet("language")){
    qDebug() << "Setting default language to " << parser.value("language");
    settings.setValue(language_key, parser.value("language"));
  }
  QString settings_language = settings.value(language_key).toString();
  if(debug_enabled) {
    qDebug() << "Language saved in settings: " << settings_language << settings.fileName();
  }

  QTranslator myappTranslator;
  configureTranslator(a, parser, settings_language, myappTranslator);


  // Check for multiple instances
  // GUID from http://www.guidgenerator.com/online-guid-generator.aspx
  /*
     QSharedMemory shared("6b50960df-f5f3-4562-bbdc-84c3bc004ef4");

     if( !shared.create( 512, QSharedMemory::ReadWrite) ) { // An instance is already running. Quit
     the current instance QMessageBox msgBox;
     msgBox.setText( QObject::tr("Can't start more than one instance of the application.") );
     msgBox.setIcon( QMessageBox::Critical );
     msgBox.exec(); exit(0); } else { */
  if(debug_enabled) {
    qDebug() << "Nitrokey App " CMAKE_BUILD_TYPE " " GUI_VERSION " (git: " GIT_VERSION ")";
    qDebug() << "Application started successfully.";
  }
  // }

  /*
     SplashScreen *splash = 0; splash = new SplashScreen; splash->show();

     QFile qss( ":/qss/default.qss" ); if( ! qss.open( QIODevice::ReadOnly ) ) { qss.setFileName(
     ":/qss/default.qss" ); qss.open(
     QIODevice::ReadOnly ); }

     if( qss.isOpen() ) { a.setStyleSheet( qss.readAll() ); }

     QTimer::singleShot(3000,splash,SLOT(deleteLater())); */


  configureRandomGenerator();
  a.setQuitOnLastWindowClosed(false);


  MainWindow w;
  if (parser.isSet("admin")){
    w.enable_admin_commands();
  }
  auto debug_file_option_name = "debug-file";
  if (parser.isSet(debug_file_option_name) && !parser.value(debug_file_option_name).isEmpty()){
    w.set_debug_file(parser.value(debug_file_option_name));
  }
  if (parser.isSet("debug-window")){
    w.set_debug_window();
  }
  if(debug_enabled){
    w.set_debug_mode();
  }

#ifdef __APPLE__
  w.set_commands_delay(100);
#endif

  if (parser.isSet("delay")){
    auto delay_in_ms = parser.value("delay").toInt();
    w.set_commands_delay(delay_in_ms);
  }

//  csApplet()->setParent(&w);
  //TODO add global exception catch for logging?
  //or use std::terminate
  int retcode;
#ifdef _NDEBUG
  try{
    retcode = a.exec();
  }
  catch (std::exception &e){
    auto message = QApplication::tr("Critical error encountered. Please restart application.\nMessage: ") + e.what();
    csApplet()->warningBox(message);
    qDebug() << message;
  }
#else
  retcode = a.exec();
  qDebug() << "normal exit";
#endif
  return retcode;
}

void configureRandomGenerator() {
    QDateTime local(QDateTime::currentDateTime());
    qsrand(static_cast<uint> (local.currentMSecsSinceEpoch() % 2000000000));
  }

void configureTranslator(const QApplication &a, const QCommandLineParser &parser, const QString &settings_language,
                         QTranslator &myappTranslator) {
  bool success = false;

#if QT_VERSION >= 0x040800 && !defined(Q_WS_MAC)
  QLocale loc = QLocale::system();
  QString lang = QLocale::languageToString(loc.language());
  if (parser.isSet("debug")) {
    qDebug() << loc << lang << loc.name();
  }
  if (!parser.isSet("language") && settings_language.isEmpty()) {
    success = myappTranslator.load(QLocale::system(), // locale
                                   "",                // file name
                                   "nitrokey_",       // prefix
                                   ":/i18n/",         // folder
                                   ".qm");            // suffix
  }

  if (!success) {
    auto translation_paths = {
        QString("/i18n/nitrokey_%1.qm").arg(settings_language),
        QString("/i18n/nitrokey_%1.qm").arg(QLocale::system().name()),
        QString("/i18n/nitrokey_%1.qm").arg(lang.toLower()),
        QString("/i18n/nitrokey_%1.qm").arg("en"),
    };

    if (parser.isSet("debug")) {
      qDebug() << "Loading translation files";
    }
    for (auto path : translation_paths) {
      for (auto p : {QString(':'), QString('.')}) {
        auto path2 = p + path;
        success = myappTranslator.load(path2);
        QFileInfo fileInfo(path2);
        if (parser.isSet("debug")) {
          qDebug() << path2 << " - file loaded successfully: " <<success
                   << ", file exists on disk: "<< fileInfo.exists();
        }
        if (success) break;
      }
      if (success) break;
    }
  }
#else
  success = myappTranslator.load(QString(":/i18n/nitrokey_%1.qm").arg(QLocale::system().name()));
#endif

  if (success) {
    a.installTranslator(&myappTranslator);
  }
}

void issue_43_workaround() {
// workaround for issue https://github.com/Nitrokey/nitrokey-app/issues/43
#if QT_VERSION > QT_VERSION_CHECK(5, 0, 0)
  if (qgetenv("QT_QPA_PLATFORMTHEME") == "appmenu-qt5") {
    qputenv("QT_QPA_PLATFORMTHEME", "generic");
  }
#endif

}

void configureBasicTranslator(const QApplication &a, QTranslator &qtTranslator) {
#if defined(Q_WS_WIN)
  qtTranslator.load("qt_" + QLocale::system().name());
#else
  qtTranslator.load("qt_" + QLocale::system().name(),
                    QLibraryInfo::location(QLibraryInfo::TranslationsPath));
#endif
  a.installTranslator(&qtTranslator);
}

void configureApplicationName() {
  QCoreApplication::setOrganizationName("Nitrokey");
  QCoreApplication::setOrganizationDomain("nitrokey.com");
  QCoreApplication::setApplicationName("Nitrokey App");
  QCoreApplication::setApplicationVersion(GUI_VERSION);
}

bool configureParser(const QApplication &a, QCommandLineParser &parser) {
  parser.setApplicationDescription(
      QCoreApplication::translate("main", "Nitrokey App - Manage your Nitrokey sticks"));
  parser.addHelpOption();
  parser.addVersionOption();
  parser.setSingleDashWordOptionMode(QCommandLineParser::ParseAsLongOptions);

  //keep in sync with data/bash-autocomplete/nitrokey-app
  parser.addOptions({
      {{"d", "debug"},
          QCoreApplication::translate("main", "Enable debug messages")},
      {{"df","debug-file"},
          QCoreApplication::translate("main", "Save debug log to file with name <log-file-name> (experimental)"),
          "log-file-name"},
      {{"dw","debug-window"},
          QCoreApplication::translate("main", "Save debug log to App's window (experimental)") },
      {{"s", "delay"},
          QCoreApplication::translate("main", "Set delay between commands sent to device (in ms) to <delay>"), "delay" },
      {"version-more",
          QCoreApplication::translate("main", "Show additional information about binary")},
      {{"a", "admin"},
          QCoreApplication::translate("main", "Enable extra administrative functions")},
//      {"lock-hardware", //TODO add functionality
//          QCoreApplication::translate("main", "Show hardware lock action in tray menu")},
      {"language-list",
          QCoreApplication::translate("main", "List available languages")},
      {{"l", "language"},
          QCoreApplication::translate("main",
                                      "Load translation file with given name "
                                      "and store this choice in settings file."),
          "en"},
});

  parser.process(a);

  if(parser.isSet("language-list")){
    QDir langDir(":/i18n/");
    auto list = langDir.entryList();
    for (auto &&translationFile : list) {
      qDebug() << translationFile.remove("nitrokey_").remove(".qm");
    }
    return true;
  }

  if(parser.isSet("version-more")){
    qDebug() << CMAKE_BUILD_TYPE << GUI_VERSION << GIT_VERSION;
    qDebug() << CMAKE_CXX_COMPILER << CMAKE_CXX_FLAGS;
    return true;
  }
  return false;
}

