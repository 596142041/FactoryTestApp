#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QSettings>
#include <QJSEngine>
#include <QThread>
#include <QComboBox>
#include <QListWidget>
#include <QLabel>
#include <QPushButton>
#include <QLineEdit>

#include <QSqlTableModel>
#include <QModelIndex>

#include "SessionManager.h"
#include "TestMethodManager.h"
#include "Logger.h"
#include "JLinkManager.h"
#include "TestClient.h"
#include "TestFixtureWidget.h"
#include "SessionInfoWidget.h"
#include "DutInfoWidget.h"
#include "ActionHintWidget.h"

class MainWindow : public QWidget
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = Q_NULLPTR);
    ~MainWindow() Q_DECL_OVERRIDE;

public slots:

    void startNewSession();
    void finishSession();
    void startFullCycleTesting();
    void resetDutListInScriptEnv();
    void delay(int msec);
    Dut getDut(int no);

private:

    QJSValue evaluateScriptFromFile(const QString& scriptFileName);
    QList<QJSValue> evaluateScriptsFromDirectory(const QString& directoryName);
    QJSValue runScript(const QString& scriptName, const QJSValueList& args);

    //QString _workDirectory = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation); // For release version
    QString _workDirectory = QDir(".").absolutePath(); //For test version
//    QString _workDirectory = QDir("../..").absolutePath(); //For development
    QSettings* _settings;
    QJSEngine* _scriptEngine;
    SessionManager* _session;
    TestMethodManager* _testSequenceManager;
    Logger* _logger;

    QList<QThread*> _threads;
    QList<JLinkManager*> _JLinkList;
    QList<TestClient*> _testClientList;

    QStringList _operatorList;

    QSqlTableModel  *model;

    //--- GUI Elements ------------------------------------------------

    QLabel* _headerLabel;
    QLineEdit* _operatorNameEdit;
    QLineEdit* _batchNumberEdit;
    QLineEdit* _batchInfoEdit;
    QPushButton* _newSessionButton;
    QPushButton* _finishSessionButton;
    QComboBox* _selectMetodBox;
    QListWidget* _testFunctionsListWidget;
    QPushButton* _startFullCycleTestingButton;
    QPushButton* _startSelectedTestButton;

    QListWidget* _logWidget;
    QPushButton* _clearLogWidgetButton;
    QPushButton* _copyLogWidgetButton;
    QListWidget* _childProcessOutputLogWidget;

    TestFixtureWidget* _testFixtureWidget;
    SessionInfoWidget* _sessionInfoWidget;
    DutInfoWidget* _dutInfoWidget;

    ActionHintWidget* _actionHintWidget;
    const QString HINT_START = "Place DUTs into the test fixture, enter information for the Step 1 and start test session";
    const QString HINT_CHOOSE_METHOD = "Choose a test method (Step 2), select DUTs (Step 3) and run full cycle testing (or single step testing) for the selected DUTs";
    const QString HINT_DETECT_DUTS = "Detecting DUTs in the test fixture...";
    const QString HINT_DOWNLOAD_RAILTEST = "Downloading Railtest...";
    const QString HINT_DEVICE_ID = "Reading device IDs...";
    const QString HINT_READY = "READY";
};

#endif // MAINWINDOW_H
