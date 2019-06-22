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
#include "../main_control/SCCDeviceParams.h"
#include "../main_control/SCCLog.h"

using namespace std;

#define MAX_BUFFER_IN   2048
#define MY_DEVICE_NAME  DEVICE_RFID_BOMBERO

static bool st_bSendMsgView = false;
static bool st_bRcvMsgView  = true;

SCCCommPort commPort;
CSocket sckComPort;
SCCLog globalLog(std::cout);
bool gl_bVerbose(true);

bool bConnected     = false;

std::string firstMessage()
{
    std::stringstream ss;

    ss << FRAME_START_MARK;
    ss << DEVICE_NAME << ":" << DEVICE_RFID_BOMBERO << ",";
    ss << SERVICE_PID << ":" << getpid() << ",";
    ss << PARAM_COM_PORT << ":" << commPort.getComPort();
    ss << FRAME_STOP_MARK;

    return std::string(ss.str());
}

void printMsg(const std::string& msg = "")
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
    std::string         nPort;
    int baudRate        = 9600;
    float fTimeFactor   = 1.0;
    int remotePort      = 0;

    char chBufferIn[MAX_BUFFER_IN];
    size_t posBuf       = 0;
    bool bOneTime       = false;
    int iBeepElapsed    = 25;

    if (argc > 2)
    {
        nPort = argv[1];
        baudRate = std::stoi(argv[2]);
        if (argc > 3)
            remotePort = std::stoi(argv[3]);
        if (argc > 4)
        {
            std::string strArg(argv[4]);
            //if (std::all_of(strArg.begin(), strArg.end(), ::isdigit))
            if (strArg.find_first_not_of("0123456789.-") == std::string::npos)
            {
                float f = std::stof(argv[4]);
                if (f > 0.0)
                    fTimeFactor = f;
            }
            else
                if (strArg == "ViewSend")
                    st_bSendMsgView = true;
        }
        if ( argc > 5)
        {
            iBeepElapsed    = std::stoi(argv[5]);
        }
        if ( argc > 6)
        {
            std::string strArg(argv[6]);
            if (strArg == "true")
                bOneTime = true;
        }
    }

    std::cout << "Starting " << DEVICE_RFID_BOMBERO << " with parameters:" << std::endl << std::endl;
    std::cout << "Com Port: "           << nPort << std::endl;
    std::cout << "Baud Rate: "          << baudRate << std::endl;
    std::cout << "Socket Remote Port: " << remotePort << std::endl;
    std::cout << "Timer Factor: "       << fTimeFactor << std::endl;
    std::cout << "View Data Sent: "     << st_bSendMsgView << std::endl;
    std::cout << "Beep Time: "          << iBeepElapsed << std::endl;
    std::cout << "One Operation: "      << bOneTime << std::endl << std::endl;

    if (remotePort)
    {
        sckComPort.connect("127.0.0.1", remotePort);
        bConnected  = false;
    }

    commPort.setDeviceName(MY_DEVICE_NAME);
    commPort.setArgs(argc, &argv[0]);
    SCCRfidUserProtocol rfidUserProtocol;
    SCCRealTime clock;

    //commPort.openPort(nPort, baudRate);
    commPort.setDeviceName(MY_DEVICE_NAME);
    commPort.setBaudRate(baudRate);
    commPort.getComPortList(nPort);

    char bufferOut[255];
    char bufferIn[250];
    //char len;
    char chLen = 0;
    int iAddr = 1;
    std::string msg;

    //msg = rfidUserProtocol.getStrCmdStatusCheck(iAddr, bufferOut, len);
    //msg = rfidUserProtocol.getCmdReadRegisters(iAddr, bufferOut, len, startReg, numRegs);
    rfidUserProtocol.getCmdSWVersion(iAddr, bufferOut, chLen);

    //rfidUserProtocol.getCmdReadRegisters(iAddr, bufferOut, len);

    msg = rfidUserProtocol.convChar2Hex(bufferOut, chLen);

    if (st_bSendMsgView)
        std::cout << "Message: " << msg << " bin: "<< bufferOut << " sent." << std::endl;

    /*commPort.sendData(bufferOut, chLen);
    commPort.sleepDuringTxRx(len+4+11);*/

    if (st_bSendMsgView)
        cout << "Waiting for response" << std::endl;
    msg = "";

    int iTimeOut;
    //char chLenLast = 0;
    int iNoRxCounter = 0;
    int iSendCount   = 0;
    //bool bRespOk     = false;
    //bool bSendStatus = false;
    char simcard = 0x30;
    do
    {
        /*if (!bConnected && sckComPort.isConnected())
            printMsg();*/
        iTimeOut = 50;
        if (iNoRxCounter >= 2)
        {
            iNoRxCounter = 0;
            //rfidUserProtocol.getStrCmdStatusCheck(iAddr, bufferOut, chLen);
            //rfidUserProtocol.getCmdReadRegisters(iAddr, bufferOut, chLen, startReg, numRegs);
            if (rfidUserProtocol.isVersionDetected()){
                rfidUserProtocol.getCmdSerialNumber(iAddr, bufferOut, chLen);
                //rfidUserProtocol.getCmdActiveCPUCard(iAddr, bufferOut, chLen);
                simcard++;
                if (simcard > 0x33)
                    simcard = 0x30;}
            else
                rfidUserProtocol.getCmdSWVersion(iAddr, bufferOut, chLen);
        }
        if (chLen > 0)
        {
            if (!commPort.isDeviceConnected() && !commPort.searchNextPort())
                break;
            if (st_bSendMsgView)
            {
                cout << commPort.printCounter() << std::endl;
                msg = rfidUserProtocol.convChar2Hex(bufferOut, chLen);
                cout << SCCRealTime::getTimeStamp() << ',' << "Sending Message: " << msg << " binary: "<< bufferOut << std::endl;
            }
            commPort.sendData(bufferOut, chLen);
            commPort.sleepDuringTxRx(chLen+4+11);
            //chLenLast = chLen;
            chLen = 0;
            //iTimeOut = 20;
        }
        while (commPort.isRxEvent() == true)
        {
            iNoRxCounter = 0;
            int iLen;
            bool ret = commPort.getData(bufferIn, iLen);
            if (st_bSendMsgView)
            {
                msg = rfidUserProtocol.convChar2Hex(bufferIn, iLen);
                cout << " bufferIn.len(): " << iLen << ". bufferIn(char): [" << msg << "]" << std::endl;
            }
            if (ret == true)
            {
                if (posBuf + iLen < MAX_BUFFER_IN)
                {
                    memcpy(&chBufferIn[posBuf], bufferIn, iLen);
                    posBuf += iLen;
                }
                while (rfidUserProtocol.findStartFrame(chBufferIn, posBuf))
                {
                    /*len = (char)iLen;
                    if (st_bRcvMsgView)
                    {
                        msg = rfidUserProtocol.convChar2Hex(bufferIn, len);
                        cout << " Buffer In(Hex): [" << msg << "]. Buffer In(char): [" << bufferIn << "]" << std::endl;
                    */
                    std::string strCmd;
                    bool bIsValidResponse = rfidUserProtocol.getRfidUserResponse(iAddr, chBufferIn, posBuf);
                    if (bIsValidResponse == true)
                    {
                        commPort.setDeviceConnected();
                        commPort.stopSearchPort();
                        posBuf = 0;
                        if (st_bRcvMsgView)
                        {
                            //cout << ++nCount << " " << commPort.printCounter() << clock.getTimeStamp() << " Valid WGT Response" << std::endl;
                            /*if (strCmd == CMD_CHECKSTATUS)
                                cout << ++nCount << " WGT Status: " << rfidUserProtocol.getStrStatus(resp[0]) << endl;*/
                        }
                        if (st_bRcvMsgView)
                        {
                            //std::stringstream ss;
                            //ss << ++nCount << " " << commPort.printCounter() << rfidUserProtocol.printStatus(iAddr) << std::endl;
                            if (rfidUserProtocol.isCardDetected() || rfidUserProtocol.isDetectEvent() || iSendCount > 4)
                            {
                                printMsg(rfidUserProtocol.printStatus(iAddr));
                                iSendCount = 0;
                                if ((!rfidUserProtocol.isBeepSoundDetected() && rfidUserProtocol.isCardDetected()) || rfidUserProtocol.isCardChanged())
                                {
                                    rfidUserProtocol.getCmdBeepSound(iAddr, bufferOut, chLen,0x01, iBeepElapsed);
                                }
                            }
                            else
                            {
                                ++iSendCount;
                                rfidUserProtocol.clearBeepSoundStatus();
                            }
                        }
                        if (st_bSendMsgView)
                            rfidUserProtocol.printData();
                        if (bOneTime)
                            break;
                    }
                    else
                    {
                        iTimeOut = 0;
                        commPort.sleepDuringTxRx(4+11);
                        break;
                    }
                }
                if (bOneTime)
                    break;
            }
        }
        if (bOneTime)
            break;
        if (iTimeOut > 0)
        {
            int tmOut = (int)(iTimeOut*fTimeFactor);
            if (!tmOut)
                tmOut = 1;
            std::this_thread::sleep_for(std::chrono::milliseconds(tmOut));
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
