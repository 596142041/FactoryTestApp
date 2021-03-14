var SLOTS_NUMBER = 3
methodManager.addMethod("OLC Zhaga STD");

ZhagaSTD =
{
    downloadRailtest: function ()
    {
        for (var slot = 1; slot < SLOTS_NUMBER + 1; slot++)
        {
            for (var i = 0; i < testClientList.length; i++)
            {
                let testClient = testClientList[i];
                let jlink = jlinkList[i];
                if(testClient.isDutAvailable(slot) && testClient.isDutChecked(slot))
                {
                    testClient.powerOn(slot);
                    delay(1000);
                    testClient.switchSWD(slot);

                    jlink.selectByUSB();
                    jlink.open();
                    jlink.setDevice("EFR32FG12PXXXF1024");
                    jlink.select();
                    jlink.setSpeed(5000);
                    jlink.connect();
//                    if(jlink.erase() < 0)
//                    {
//                        return;
//                    }

//                    jlink.downloadFile("sequences/OLCZhagaECO/dummy_btl_efr32xg12.s37", 0);
//                    jlink.downloadFile("sequences/OLCZhagaECO/olc_zhaga_2l4l_railtest.hex", 0);
//                    jlink.reset();
//                    jlink.go();
                    jlink.close();
                }
            }
        }
    },

    //---

    downloadSoftware: function ()
    {
        for (var slot = 1; slot < SLOTS_NUMBER + 1; slot++)
        {
            for (var i = 0; i < testClientList.length; i++)
            {
                let testClient = testClientList[i];
                let jlink = jlinkList[i];
                if(testClient.isDutAvailable(slot) && testClient.isDutChecked(slot))
                {
                    testClient.powerOn(slot);
                    delay(1000);
                    testClient.switchSWD(slot);

                    jlink.selectByUSB();
                    jlink.open();
                    jlink.setDevice("EFR32FG12PXXXF1024");
                    jlink.select();
                    jlink.setSpeed(5000);
                    jlink.connect();
//                    if(jlink.erase() < 0)
//                    {
//                        return;
//                    }

//                    jlink.downloadFile("sequences/OLCZhagaECO/olc_zhaga_software.hex", 0);
                    jlink.close();
                }
            }
        }
    },

    testRadio: function ()
    {
        testClient.commandSequenceStarted();

        for(var slot = 1; slot < testClient.dutsCount() + 1; slot++)
        {
            if(testClient.isDutAvailable(slot) && testClient.isDutChecked(slot))
            {
                testClient.on_testRadio(slot);
            }
        }

        testClient.commandSequenceFinished();
    },

    //---

    checkAinVoltage: function ()
    {
        for (var slot = 1; slot < SLOTS_NUMBER + 1; slot++)
        {
            for (var i = 0; i < testClientList.length; i++)
            {
                let testClient = testClientList[i];
                let voltage = testClient.readAIN(slot, 1, 0);
                if(voltage > 70000 && voltage < 72000)
                {
                    testClientList[i].setDutProperty(slot, "voltageChecked", true);
                    logger.logSuccess("Voltage (3.3V) on AIN 1 for DUT " + testClientList[i].dutNo(slot) + " checked.");
                }
                else
                {
                    testClientList[i].setDutProperty(slot, "voltageChecked", false);
                    testClientList[i].addDutError(slot, response.join(' '));
                    logger.logDebug("Error voltage value on AIN 1 : " + voltage  + ".");
                    logger.logError("Error voltage value on AIN 1 detected! DUT " + testClientList[i].dutNo(slot));
                }
            }
        }
    },

    //---

    startTesting: function ()
    {
//        testClient.commandSequenceStarted();

        GeneralCommands.testConnection();
        GeneralCommands.detectDuts();
        ZhagaECO.downloadRailtest();
        GeneralCommands.readChipId();
        ZhagaECO.checkAinVoltage();
        GeneralCommands.testAccelerometer();
        GeneralCommands.testLightSensor();
        GeneralCommands.testDALI();
        GeneralCommands.testGNSS();
        ZhagaECO.checkTestingCompletion();
        ZhagaECO.downloadSoftware();

//        testClient.commandSequenceFinished();
    },

    //---

    checkTestingCompletion: function ()
    {
        for(var slot = 1; slot < SLOTS_NUMBER + 1; slot++)
        {
            for (var i = 0; i < testClientList.length; i++)
            {
                let testClient = testClientList[i];
                if(testClient.isDutAvailable(slot) && testClient.isDutChecked(slot))
                {
                    if( testClient.dutProperty(slot, "id") !== "" &&
                            testClient.dutProperty(slot, "voltageChecked") &&
                            testClient.dutProperty(slot, "lightSensChecked") &&
                            testClient.dutProperty(slot, "daliChecked") &&
                            //testClient.dutProperty(slot, "gnssChecked") &&
                            //testClient.dutProperty(slot, "radioChecked") &&
                            testClient.dutProperty(slot, "accelChecked")
                            )
                    {
                        testClient.setDutProperty(slot, "state", 2);
                    }

                    else
                    {
                        testClient.setDutProperty(slot, "state", 3);
                    }

                    testClient.slotFullyTested(slot);
                }
            }
        }
    }
}

methodManager.addFunctionToGeneralList("Full cycle testing", ZhagaSTD.startTesting);
methodManager.addFunctionToGeneralList("Test connection to JLink", GeneralCommands.testConnection);
methodManager.addFunctionToGeneralList("Detect DUTs", GeneralCommands.detectDuts);
methodManager.addFunctionToGeneralList("Download Railtest", ZhagaSTD.downloadRailtest);
methodManager.addFunctionToGeneralList("Read CSA", GeneralCommands.readCSA);
methodManager.addFunctionToGeneralList("Supply power to DUTs", GeneralCommands.powerOn);
methodManager.addFunctionToGeneralList("Power off DUTs", GeneralCommands.powerOff);
methodManager.addFunctionToGeneralList("Read unique device identifiers (ID)", GeneralCommands.readChipId);
methodManager.addFunctionToGeneralList("Check voltage on AIN 1 (3.3V)", ZhagaSTD.checkAinVoltage);
methodManager.addFunctionToGeneralList("Test accelerometer", GeneralCommands.testAccelerometer);
methodManager.addFunctionToGeneralList("Test light sensor", GeneralCommands.testLightSensor);
methodManager.addFunctionToGeneralList("Test DALI", GeneralCommands.testDALI);
//methodManager.addFunctionToGeneralList("Test radio interface", ZhagaECO.testRadio, true);
methodManager.addFunctionToGeneralList("Test GNSS", GeneralCommands.testGNSS);
methodManager.addFunctionToGeneralList("Check Testing Completion", ZhagaSTD.checkTestingCompletion);
methodManager.addFunctionToGeneralList("Download Software", ZhagaSTD.downloadSoftware);
