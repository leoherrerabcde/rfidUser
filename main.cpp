#include <iostream>
#include <thread>
#include <chrono>
#include <string>
#include <sstream>
#include <algorithm>

#include "SCCRfidUserProtocol.h"

#include "../commPort/SCCCommPort.h"
#include "../commPort/SCCRealTime.h"
#include "../commPort/SCCArgumentParser.h"

#include "../main_control/CSocket.h"
#include "../main_control/SCCDeviceNames.h"
#include "../main_control/SCCLog.h"

using namespace std;

#define MAX_BUFFER_IN   2048

static bool st_bSendMsgView = true;
static bool st_bRcvMsgView  = true;

CSocket sckComPort;
SCCLog glLog(std::cout);

bool bConnected     = false;

std::string firstMessage()
{
    std::stringstream ss;

    ss << FRAME_START_MARK;
    ss << DEVICE_NAME << ":" << DEVICE_RFID_BOMBERO << ",";
    ss << SERVICE_PID << ":" << getpid();
    ss << FRAME_STOP_MARK;

    return std::string(ss.str());
}

void printMsg(std::string msg)
{
    if (sckComPort.isConnected())
    {
        if (bConnected == false)
        {
            bConnected = true;
            sckComPort.sendData(firstMessage());
            std::cout << "Socket connected." << std::endl;
        }
        sckComPort.sendData(msg);
    }
    else
    {
        std::cout << SCCRealTime::getTimeStamp() << ',' << msg << std::endl;
    }
}

int main(int argc, char* argv[])
{
    int nPort           = 7;
    int baudRate        = 9600;
    float fTimeFactor   = 1.0;
    int remotePort      = 0;
    int startReg        = 1;
    int numRegs         = MAX_REGISTERS;
    char chBufferIn[MAX_BUFFER_IN];
    size_t posBuf       = 0;
    bool bOneTime       = false;

    if (argc > 2)
    {
        nPort = std::stoi(argv[1]);
        baudRate = std::stoi(argv[2]);
        if (argc > 3)
            remotePort = std::stoi(argv[3]);
        if (argc > 4)
        {
            std::string strArg(argv[4]);
            if (std::all_of(strArg.begin(), strArg.end(), ::isdigit))
                fTimeFactor = std::stof(argv[4]);
            else
                if (strArg == "ViewSend")
                    st_bSendMsgView = true;
        }
        if ( argc > 6)
        {
            startReg    = std::stoi(argv[5]);
            numRegs     = std::stoi(argv[6]);
        }
        if (argc > 7)
        {
            std::string strArg(argv[7]);
            if (strArg == "true")
                bOneTime = true;
        }
    }

    if (remotePort)
    {
        sckComPort.connect("127.0.0.1", remotePort);
        bConnected  = false;
    }
    SCCCommPort commPort;
    SCCRfidUserProtocol rfidUserProtocol;
    SCCRealTime clock;

    commPort.openPort(nPort, baudRate);

    char bufferOut[255];
    char bufferIn[250];
    char len;
    int iAddr = 1;
    std::string msg;

    //msg = rfidUserProtocol.getStrCmdStatusCheck(iAddr, bufferOut, len);
    //msg = rfidUserProtocol.getCmdReadRegisters(iAddr, bufferOut, len, startReg, numRegs);
    rfidUserProtocol.getCmdSWVersion(iAddr, bufferOut, len);

    //rfidUserProtocol.getCmdReadRegisters(iAddr, bufferOut, len);

    msg = rfidUserProtocol.convChar2Hex(bufferOut, len);

    if (st_bSendMsgView)
        std::cout << "Message: " << msg << " bin: "<< bufferOut << " sent." << std::endl;

    commPort.sendData(bufferOut, len);
    commPort.sleepDuringTxRx(len+numRegs*4+11);

    if (st_bSendMsgView)
        cout << "Waiting for response" << std::endl;
    msg = "";

    int iTimeOut;
    bool bNextAddr;
    char chLen = 0;
    //char chLenLast = 0;
    int iNoRxCounter = 0;
    do
    {
        bNextAddr = true;
        iTimeOut = 250;
        if (iNoRxCounter >= 5)
        {
            iNoRxCounter = 0;
            //rfidUserProtocol.getStrCmdStatusCheck(iAddr, bufferOut, chLen);
            //rfidUserProtocol.getCmdReadRegisters(iAddr, bufferOut, chLen, startReg, numRegs);
            rfidUserProtocol.getCmdSWVersion(iAddr, bufferOut, len);
        }
        if (chLen > 0)
        {
            if (st_bSendMsgView)
            {
                cout << commPort.printCounter() << std::endl;
                msg = rfidUserProtocol.convChar2Hex(bufferOut, chLen);
                cout << SCCRealTime::getTimeStamp() << ',' << "Sending Message: " << msg << " binary: "<< bufferOut << std::endl;
            }
            commPort.sendData(bufferOut, chLen);
            commPort.sleepDuringTxRx(chLen+numRegs*4+11);
            //chLenLast = chLen;
            chLen = 0;
            //iTimeOut = 20;
            bNextAddr = false;
        }
        if (commPort.isRxEvent() == true)
        {
            iNoRxCounter = 0;
            bNextAddr =false;
            int iLen;
            bool ret = commPort.getData(bufferIn, iLen);
            if (st_bSendMsgView)
                cout << " bufferIn.len(): " << iLen << ". bufferIn(char): [" << bufferIn << "]" << std::endl;

            if (ret == true)
            {
                if (posBuf + iLen < MAX_BUFFER_IN)
                {
                    memcpy(&chBufferIn[posBuf], bufferIn, iLen);
                    posBuf += iLen;
                }
                //len = (char)iLen;
                /*if (st_bRcvMsgView)
                {
                    msg = rfidUserProtocol.convChar2Hex(bufferIn, len);
                    cout << ++nCount << " Buffer In(Hex): [" << msg << "]. Buffer In(char): [" << bufferIn << "]" << std::endl;
                }*/
                std::string strCmd;
                //char resp[256];
                //int addr = iAddr;
                //char respLen = 0;
                bool bIsValidResponse = rfidUserProtocol.getFlowMeterResponse(iAddr, chBufferIn, posBuf);
                //bool bNextAction = false;
                if (bIsValidResponse == true)
                {
                    posBuf = 0;
                    //iTimeOut = 50;
                    if (st_bRcvMsgView)
                    {
                        //cout << ++nCount << " " << commPort.printCounter() << clock.getTimeStamp() << " Valid WGT Response" << std::endl;
                        /*if (strCmd == CMD_CHECKSTATUS)
                            cout << ++nCount << " WGT Status: " << rfidUserProtocol.getStrStatus(resp[0]) << endl;*/
                    }
                    /*bNextAction = rfidUserProtocol.nextAction(iAddr, bufferOut, chLen, iTimeOut);
                    if (bNextAction == true)*/
                        if (st_bRcvMsgView)
                        {
                            //std::stringstream ss;
                            //ss << ++nCount << " " << commPort.printCounter() << rfidUserProtocol.printStatus(iAddr) << std::endl;
                            printMsg(rfidUserProtocol.printStatus(iAddr));
                        }
                    if (st_bSendMsgView)
                        rfidUserProtocol.printData();
                    if (bOneTime)
                        break;
                }
                else
                {
                    iTimeOut = 0;
                    commPort.sleepDuringTxRx(numRegs*4+11-posBuf);
                }
            }
        }
        if (iTimeOut > 0)
            std::this_thread::sleep_for(std::chrono::milliseconds((int)(iTimeOut*fTimeFactor)));
        if (bNextAddr == true)
        {
            //++iAddr;
        }
        if (/*bConnecting ==true || */bConnected == true)
        {
            if (sckComPort.getState() == sckError)
            {
                if (remotePort)
                {
                    sckComPort.connect("127.0.0.1", remotePort);
                    //bConnecting = true;
                    bConnected  = false;
                    //iSckCounter = 0;
                }
            }
            /*if (sckComPort.isConnected())
            {
                //bConnecting = false;
                bConnected  = true;
            }*/
        }
        ++iNoRxCounter;
        if (bConnected == true && !sckComPort.isConnected())
            break;
    }
    while (commPort.isOpened());

    sckComPort.disconnect();
    exit(0);
    commPort.closePort();

    return 0;
}
