#include "MainWindow.h"

#include <QCoreApplication>
#include <QStandardPaths>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QCompleter>
#include <QSerialPortInfo>

MainWindow::MainWindow(QWidget *parent)
    : QWidget(parent)
{
    thread()->setObjectName("Main Window thread");
    setStyleSheet("color: #424242; font-size:10pt;");    

    _settings = new QSettings(_workDirectory + "/settings.ini", QSettings::IniFormat, this);
    _settings->setValue("workDirectory", _workDirectory);

    _scriptEngine = _scriptEngine = new QJSEngine(this);
    _scriptEngine->installExtensions(QJSEngine::ConsoleExtension);

    _scriptEngine->globalObject().setProperty("mainWindow", _scriptEngine->newQObject(this));

    _session = new SessionManager(_settings, this);
    _scriptEngine->globalObject().setProperty("session", _scriptEngine->newQObject(_session));

    _testSequenceManager = new TestMethodManager(this);
    _logger = new Logger(_settings, _session, this);
    _scriptEngine->globalObject().setProperty("testSequenceManager", _scriptEngine->newQObject(_testSequenceManager));

    evaluateScriptFromFile(_workDirectory + "/init.js");
    evaluateScriptsFromDirectory(_workDirectory + "/sequences");
    _scriptEngine->globalObject().setProperty("logger", _scriptEngine->newQObject(_logger));

    _printerManager = new PrinterManager(_settings, this);
    _printerManager->setLogger(_logger);

//    auto availablePorts = QSerialPortInfo::availablePorts();
//    for(auto & portInfo : availablePorts)
//    {
//        qDebug() << portInfo.portName() << portInfo.productIdentifier();
//    }

    //Setting number of active test panels (max - 5)
    const int MAX_PANELS_COUNT = 5;

    // Creating objects for controlling JLinks & Test clients
    for (int i = 0; i < MAX_PANELS_COUNT; i++)
    {
        if(_settings->value(QString("TestBoard/state" + QString().setNum(i + 1))).toBool())
        {
            // Creating threads for run the tests for each test panel
            auto newThread = new QThread(this);
            newThread->setObjectName(QString("Thread %1").arg(i + 1));
            _threads.push_back(newThread);

            auto newJlink = new JLinkManager(_settings);
            newJlink->setSN(_settings->value(QString("JLink/SN" + QString().setNum(i + 1))).toString());
            newJlink->setLogger(_logger);
            _JLinkList.push_back(newJlink);
            _JLinkList.last()->moveToThread(_threads.last());
            QJSValue jlink = _scriptEngine->newQObject(newJlink);
            _scriptEngine->globalObject().property("JlinkList").setProperty(i, jlink);

            auto testClient = new TestClient(_settings, _session);
            testClient->setLogger(_logger);
            _testClientList.push_back(testClient);
            _testClientList.last()->setDutsNumbers(_settings->value(QString("TestBoard/duts" + QString().setNum(i + 1))).toString());
            _testClientList.last()->setPort(_settings->value(QString("Railtest/serial%1").arg(QString().setNum(i + 1))).toString());

            if(_settings->value("multithread").toBool())
                _testClientList.last()->moveToThread(_threads.last());

            _scriptEngine->globalObject().property("testClientList").setProperty(i, _scriptEngine->newQObject(testClient));

            _threads.last()->start();
            _testClientList.last()->open();
            delay(100);
        }
    }

//--- GUI Layouts---
    QVBoxLayout* mainLayout = new QVBoxLayout;
    setLayout(mainLayout);

    mainLayout->addSpacing(30);
    QHBoxLayout* headerLayout = new QHBoxLayout;
    mainLayout->addLayout(headerLayout);
    mainLayout->addSpacing(30);

    QHBoxLayout* panelsLayout = new QHBoxLayout;
    mainLayout->addLayout(panelsLayout);

    QVBoxLayout* leftPanelLayout = new QVBoxLayout;
    panelsLayout->addLayout(leftPanelLayout);

//    QVBoxLayout* middlePanelLayout = new QVBoxLayout;
//    panelsLayout->addLayout(middlePanelLayout);
    panelsLayout->addSpacing(30);

    QVBoxLayout* rightPanelLayout = new QVBoxLayout;
    panelsLayout->addLayout(rightPanelLayout);

    QHBoxLayout* logLayout = new QHBoxLayout;
    mainLayout->addLayout(logLayout);

    //Header info
    headerLayout->addStretch();
    _headerLabel = new QLabel("HERE WE PLACE HEADER INFO", this);
    _headerLabel->setStyleSheet("font-size:10pt; font-weight: bold;");
    headerLayout->addWidget(_headerLabel);
    headerLayout->addStretch();

    //Next action hint
    _actionHintWidget = new ActionHintWidget(this);
    _actionHintWidget->showNormalHint(HINT_START);
    mainLayout->addWidget(_actionHintWidget);

    //Input session info and start session widgets
    leftPanelLayout->addSpacing(10);
    QLabel* sessionInfoLabel = new QLabel("<b>Step 1.</b> Enter session information", this);
    leftPanelLayout->addWidget(sessionInfoLabel);

    QFormLayout* sessionInfoLayout = new QFormLayout;
    _operatorNameEdit = new QLineEdit(this);
    _operatorNameEdit->setPlaceholderText("Enter Operator Name here");
    _operatorList = _settings->value("operatorList").toString().split("|");
    _operatorNameEdit->setCompleter(new QCompleter(_operatorList, this));
    _operatorNameEdit->completer()->setCaseSensitivity(Qt::CaseInsensitive);

    _batchNumberEdit = new QLineEdit(this);
    _batchNumberEdit->setPlaceholderText("Enter Batch number here");
    _batchInfoEdit = new QLineEdit(this);
    _batchInfoEdit->setPlaceholderText("Enter Batch info here");
    sessionInfoLayout->addRow("Operator name", _operatorNameEdit);
    sessionInfoLayout->addRow("Batch number", _batchNumberEdit);
    sessionInfoLayout->addRow("Batch information", _batchInfoEdit);
    leftPanelLayout->addLayout(sessionInfoLayout);
    leftPanelLayout->addSpacing(20);

    QHBoxLayout* sessionButtonLayout = new QHBoxLayout;
    leftPanelLayout->addLayout(sessionButtonLayout);
    _newSessionButton = new QPushButton(QIcon(QString::fromUtf8(":/icons/testOnly")), tr("Create new session"), this);
    _newSessionButton->setFixedSize(165, 40);
    _newSessionButton->setEnabled(false);
    sessionButtonLayout->addWidget(_newSessionButton);
    sessionButtonLayout->addStretch();

    _finishSessionButton = new QPushButton(QIcon(QString::fromUtf8(":/icons/finish")), tr("Finish current session"), this);
    _finishSessionButton->setFixedSize(165, 40);
    _finishSessionButton->setEnabled(false);
    sessionButtonLayout->addWidget(_finishSessionButton);

    leftPanelLayout->addSpacing(30);

    //Choose method box
    QLabel* selectSequenceBoxLabel = new QLabel("<b>Step 2.</b> Choose test method", this);
    _selectMetodBox = new QComboBox(this);
    _selectMetodBox->setEnabled(false);
    _selectMetodBox->setFixedHeight(30);

    leftPanelLayout->addWidget(selectSequenceBoxLabel);
    leftPanelLayout->addWidget(_selectMetodBox);

    //Test functions list widget
    QLabel* testFunctionsListLabel = new QLabel("Avaliable testing commands:", this);
    _testFunctionsListWidget = new QListWidget(this);
    _testFunctionsListWidget->setEnabled(false);

    leftPanelLayout->addWidget(testFunctionsListLabel);
    leftPanelLayout->addWidget(_testFunctionsListWidget);
    leftPanelLayout->addStretch();

    //Start testing buttons
    QHBoxLayout* startTestingButtonsLayout = new QHBoxLayout;
    leftPanelLayout->addLayout(startTestingButtonsLayout);
    leftPanelLayout->addSpacing(9);

    _startFullCycleTestingButton = new QPushButton(QIcon(QString::fromUtf8(":/icons/autoDownload")), tr("Start full cycle testing"), this);
    _startFullCycleTestingButton->setFixedSize(165, 40);
    _startFullCycleTestingButton->setEnabled(false);
    startTestingButtonsLayout->addWidget(_startFullCycleTestingButton);

    _startSelectedTestButton = new QPushButton(QIcon(QString::fromUtf8(":/icons/checked")), tr("Start Command"), this);
    _startSelectedTestButton->setFixedSize(165, 40);
    _startSelectedTestButton->setEnabled(false);
    startTestingButtonsLayout->addWidget(_startSelectedTestButton);

    //Session info widget

    QHBoxLayout* sessionWidgetLayout = new QHBoxLayout;
    rightPanelLayout->addLayout(sessionWidgetLayout);

    _sessionInfoWidget = new SessionInfoWidget(_session, this);
    sessionWidgetLayout->addWidget(_sessionInfoWidget);

    //Test fixture representation widget

    QHBoxLayout* dutsLayout = new QHBoxLayout;
    rightPanelLayout->addLayout(dutsLayout);
    _testFixtureWidget = new TestFixtureWidget(_session, this);
    _testFixtureWidget->setEnabled(false);
    dutsLayout->addWidget(_testFixtureWidget);

    //DUTs info widget

    QVBoxLayout* dutInfoWidgetLayout = new QVBoxLayout;
    dutsLayout->addLayout(dutInfoWidgetLayout);
    _dutInfoWidget = new DutInfoWidget(_session, this);
    dutInfoWidgetLayout->addSpacing(14);
    dutInfoWidgetLayout->addWidget(_dutInfoWidget);

    //Log widget
    _logWidget = new QListWidget(this);
    _logWidget->setFixedHeight(200);
    logLayout->addWidget(_logWidget);
    _logger->setLogWidget(_logWidget);

    QVBoxLayout* logWidgetControlsLayout = new QVBoxLayout;
    logLayout->addLayout(logWidgetControlsLayout);

    _clearLogWidgetButton = new QPushButton(QIcon(QString::fromUtf8(":/icons/clear")), "", this);
    _clearLogWidgetButton->setFixedSize(32, 32);
    _clearLogWidgetButton->setToolTip("Clear log output");
    logWidgetControlsLayout->addWidget(_clearLogWidgetButton);

    _copyLogWidgetButton = new QPushButton(QIcon(QString::fromUtf8(":/icons/copy")), "", this);
    _copyLogWidgetButton->setFixedSize(32, 32);
    _copyLogWidgetButton->setToolTip("Copy log output");
    logWidgetControlsLayout->addWidget(_copyLogWidgetButton);
    logWidgetControlsLayout->addStretch();

    //Widget for logging output of child processes
    _childProcessOutputLogWidget = new QListWidget(this);
    _childProcessOutputLogWidget->setFixedHeight(200);
    logLayout->addWidget(_childProcessOutputLogWidget);
    _logger->setChildProcessLogWidget(_childProcessOutputLogWidget);

//Connections
    connect(_operatorNameEdit, &QLineEdit::textEdited, [=](const QString& text)
    {
        if(!text.isEmpty() && !_batchNumberEdit->text().isEmpty())
        {
            _newSessionButton->setEnabled(true);
        }

        else
        {
            _newSessionButton->setEnabled(false);
        }
    });

    connect(_batchNumberEdit, &QLineEdit::textEdited, [=](const QString& text)
    {
        if(!text.isEmpty() && !_operatorNameEdit->text().isEmpty())
        {
            _newSessionButton->setEnabled(true);
        }

        else
        {
            _newSessionButton->setEnabled(false);
        }
    });

    connect(_selectMetodBox, SIGNAL(currentTextChanged(const QString&)), _testSequenceManager, SLOT(setCurrentMethod(const QString&)));
    connect(_selectMetodBox, &QComboBox::currentTextChanged, [=]()
    {
        _testFunctionsListWidget->clear();
        _testFunctionsListWidget->addItems(_testSequenceManager->currentMethodGeneralFunctionNames());
        if(_testFunctionsListWidget->count() > 0)
        {
            _testFunctionsListWidget->setCurrentItem(_testFunctionsListWidget->item(0));
        }
    });

    connect(_startSelectedTestButton, &QPushButton::clicked, [=]()
    {
        if(_testFunctionsListWidget->currentItem())
        {
            _testSequenceManager->runTestFunction(_testFunctionsListWidget->currentItem()->text());
        }
    });

    connect(_testFixtureWidget, &TestFixtureWidget::dutClicked, _dutInfoWidget, &DutInfoWidget::setDutChecked);
    connect(_testFixtureWidget, &TestFixtureWidget::dutClicked, [=](int no)
    {
        _dutInfoWidget->showDutInfo(no);
    });

    connect(_clearLogWidgetButton, &QPushButton::clicked, _logWidget, &QListWidget::clear);
    connect(_clearLogWidgetButton, &QPushButton::clicked, _childProcessOutputLogWidget, &QListWidget::clear);

    for (auto & testClient : _testClientList)
    {
        connect(testClient, &TestClient::dutChanged, _testFixtureWidget, &TestFixtureWidget::refreshButtonState, Qt::QueuedConnection);
        connect(_testFixtureWidget, &TestFixtureWidget::dutClicked, testClient, &TestClient::setDutChecked, Qt::QueuedConnection);
        connect(testClient, &TestClient::dutChanged, _dutInfoWidget, &DutInfoWidget::updateDut, Qt::QueuedConnection);
        connect(testClient, &TestClient::dutFullyTested, _session, &SessionManager::logDutInfo, Qt::QueuedConnection);

        connect(testClient, &TestClient::commandSequenceFinished, [this]()
        {
            if(_waitingThreadSequenceFinished)
            {
                _finishSignalsCount++;
            }
        });
    }

    connect(_newSessionButton, &QPushButton::clicked, this, &MainWindow::startNewSession);
    connect(_finishSessionButton, &QPushButton::clicked, this, &MainWindow::finishSession);
    connect(_startFullCycleTestingButton, &QPushButton::clicked, this, &MainWindow::startFullCycleTesting);

    connect(_session, &SessionManager::printLabel, _printerManager, &PrinterManager::addLabel);
}

MainWindow::~MainWindow()
{
    _session->writeDutRecordsToDatabase();
    for(auto & jlink : _JLinkList)
    {
        jlink->deleteLater();
    }

    for(auto & test : _testClientList)
    {
        test->deleteLater();
    }

    for(auto & thread : _threads)
    {
        thread->quit();
        thread->deleteLater();
    }

    _settings->setValue("operatorList", _operatorList.join("|"));
}

void MainWindow::startNewSession()
{
    setControlsEnabled(false);

    for(auto & jlink : _JLinkList)
    {
        jlink->establishConnection();
        delay(100);
    }

    //------------------------------------------------------------------------------------------

    _session->setOperatorName(_operatorNameEdit->text().simplified());
    _session->setStartTime(QDateTime::currentDateTime().toString());
    _session->setBatchNumber(_batchNumberEdit->text());
    _session->setBatchInfo(_batchInfoEdit->text());

    //------------------------------------------------------------------------------------------
    _selectMetodBox->setEnabled(true);
    _selectMetodBox->clear();
    _selectMetodBox->addItems(_testSequenceManager->avaliableSequencesNames());

    _testSequenceManager->setCurrentMethod(_selectMetodBox->currentText());

    _testFunctionsListWidget->setEnabled(true);
    _testFunctionsListWidget->addItems(_testSequenceManager->currentMethodGeneralFunctionNames());
    if(_testFunctionsListWidget->count() > 0)
    {
        _testFunctionsListWidget->setCurrentItem(_testFunctionsListWidget->item(0));
    }

    _startFullCycleTestingButton->setEnabled(true);
    _startSelectedTestButton->setEnabled(true);
    _testFixtureWidget->setEnabled(true);
    _finishSessionButton->setEnabled(true);

    _actionHintWidget->showNormalHint(HINT_CHOOSE_METHOD);
    _sessionInfoWidget->refresh();
    _testFixtureWidget->refreshButtonsState();

    if(!_operatorList.contains(_session->operatorName(), Qt::CaseInsensitive))
    {
        _operatorList.push_back(_session->operatorName());
    }
}

void MainWindow::finishSession()
{
    setControlsEnabled(false);
    _session->writeDutRecordsToDatabase();
    _session->clear();

    _selectMetodBox->clear();
    _testFixtureWidget->reset();
    _operatorNameEdit->clear();
    _batchNumberEdit->clear();
    _batchInfoEdit->clear();
    _logWidget->clear();
    _childProcessOutputLogWidget->clear();

    _actionHintWidget->showNormalHint(HINT_START);
    _sessionInfoWidget->refresh();
    _testFixtureWidget->refreshButtonsState();

    _operatorNameEdit->setEnabled(true);
    _batchNumberEdit->setEnabled(true);
    _batchInfoEdit->setEnabled(true);
}

void MainWindow::startFullCycleTesting()
{
    setControlsEnabled(false);

    _actionHintWidget->showProgressHint(HINT_DETECT_DUTS);
    _testFixtureWidget->reset();

    _activeTestClientsCount = _testClientList.size();
    for(auto & testClient : _testClientList)
    {
        testClient->checkDutsCurrent();
    }

    waitAllThreadsSequencesFinished();

    _activeTestClientsCount = 0;
    for(auto & testClient : _testClientList)
    {
        if(testClient->isActive())
            _activeTestClientsCount++;
    }

    _actionHintWidget->showProgressHint(HINT_DOWNLOAD_RAILTEST);
    _testSequenceManager->runTestFunction("Supply power to DUTs");
    delay(5000);

//    _testSequenceManager->runTestFunction("Download Railtest");
//    delay(12000);

    _actionHintWidget->showProgressHint(HINT_FULL_TESTING);
    for(auto & testClient : _testClientList)
    {
        if(testClient->isActive())
        {
            testClient->startTesting();
            delay(100);
        }
    }

    waitAllThreadsSequencesFinished();

//    _testSequenceManager->runTestFunction("Download Software");
//    delay(60000);

//    _actionHintWidget->showProgressHint(HINT_DEVICE_ID);
//    _testSequenceManager->runTestFunction("Read unique device identifiers (ID)");
//    delay(5000);

//    _actionHintWidget->showProgressHint(HINT_CHECK_VOLTAGE);
//    _testSequenceManager->runTestFunction("Check voltage on AIN 1 (3.3V)");
//    delay(5000);

//    _actionHintWidget->showProgressHint(HINT_TEST_ACCEL);
//    _testSequenceManager->runTestFunction("Test accelerometer");
//    delay(5000);

//    _actionHintWidget->showProgressHint(HINT_TEST_LIGHT);
//    _testSequenceManager->runTestFunction("Test light sensor");
//    delay(5000);

//    _actionHintWidget->showProgressHint(HINT_TEST_DALI);
//    _testSequenceManager->runTestFunction("Test DALI");
//    delay(10000);

//    _testSequenceManager->runTestFunction("Check Testing Completion");

    _actionHintWidget->showProgressHint(HINT_READY);
    _session->writeDutRecordsToDatabase();

    setControlsEnabled(true);
    _newSessionButton->setEnabled(false);
    _operatorNameEdit->setEnabled(false);
    _batchNumberEdit->setEnabled(false);
    _batchInfoEdit->setEnabled(false);

}

void MainWindow::resetDutListInScriptEnv()
{
//    int counter = 0;
//    for(auto & dut : _session->getDutList())
//    {
//        QJSValue currentDut = _scriptEngine->newQObject(dut);
//        _scriptEngine->globalObject().property("dutList").setProperty(counter, currentDut);
//        counter++;
//    }
}

QJSValue MainWindow::evaluateScriptFromFile(const QString &scriptFileName)
{
    QFile scriptFile(scriptFileName);
    scriptFile.open(QIODevice::ReadOnly | QIODevice::Text);
    QTextStream in(&scriptFile);
    in.setCodec("Utf-8");
    QJSValue scriptResult = _scriptEngine->evaluate(QString(in.readAll()));
    scriptFile.close();
    return scriptResult;
}

QList<QJSValue> MainWindow::evaluateScriptsFromDirectory(const QString& directoryName)
{
    QDir scriptsDir = QDir(directoryName, "*.js", QDir::Name, QDir::Files);
    QStringList fileNames = scriptsDir.entryList();
    QList<QJSValue> results;

    for (auto & i : fileNames)
    {
        results.push_back(evaluateScriptFromFile(scriptsDir.absoluteFilePath(i)));
    }

    return results;
}

QJSValue MainWindow::runScript(const QString& scriptName, const QJSValueList& args)
{
    return _scriptEngine->globalObject().property(scriptName).call(args);
}

void MainWindow::setControlsEnabled(bool state)
{
    _operatorNameEdit->setEnabled(state);
    _batchNumberEdit->setEnabled(state);
    _batchInfoEdit->setEnabled(state);

    _newSessionButton->setEnabled(state);
    _finishSessionButton->setEnabled(state);

    _selectMetodBox->setEnabled(state);
    _testFunctionsListWidget->setEnabled(state);
    _startFullCycleTestingButton->setEnabled(state);
    _startSelectedTestButton->setEnabled(state);

    _testFixtureWidget->setEnabled(state);
}

void MainWindow::waitAllThreadsSequencesFinished()
{
    if(!_settings->value("multithread").toBool())
        return;

    _waitingThreadSequenceFinished = true;
    _finishSignalsCount = 0;
    while(_finishSignalsCount < _activeTestClientsCount)
    {
        QCoreApplication::processEvents();
    }
    _waitingThreadSequenceFinished = false;
}

void MainWindow::delay(int msec)
{
    QTime expire = QTime::currentTime().addMSecs(msec);
    while (QTime::currentTime() <= expire)
    {
        QCoreApplication::processEvents();
    }
}

Dut MainWindow::getDut(int no)
{
    for (auto & testClient : _testClientList)
    {
        for(auto & dut : testClient->getDuts())
        {
            if(dut["no"].toInt() == no)
                return dut;
        }
    }

    return dutTemplate;
}
