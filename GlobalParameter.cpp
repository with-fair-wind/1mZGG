#include "GlobalParameter.h"

GlobalParameter::GlobalParameter()
{
    m_bDebugEN = false;
    m_qstrImageFormat = "bmp";
}

#include <QDebug>
GlobalParameter::~GlobalParameter()
{
    WriteInitFile();

    qDebug() << "GlobalParameter";
}

GlobalParameter* GlobalParameter::GetInstance(void)
{
    static GlobalParameter instance;
    return &instance;
}

int GlobalParameter::ReadInitFile(QString qstrFileName)
{
    /// SystemInit.ini
    QSettings* qsetSystemInit = new QSettings(qstrFileName, QSettings::IniFormat);
    if (qsetSystemInit)
    {
        m_SPath.qstrInitFileName = qstrFileName;
        /// [CommNetSettings]
        qsetSystemInit->beginGroup("CommNetSettings");
        if (qsetSystemInit->contains("PortCommDisplay")
            && qsetSystemInit->contains("PortCommExposure")
            && qsetSystemInit->contains("PortCommMasterControl")
            && qsetSystemInit->contains("PortCommServo")
            && qsetSystemInit->contains("IPAddressLocal")
            && qsetSystemInit->contains("IPAddressImageTrans")
            && qsetSystemInit->contains("PortImageTrans")
            && qsetSystemInit->contains("IPAddressMasterControl")
            && qsetSystemInit->contains("PortMasterControl")
            && qsetSystemInit->contains("IPAddressErrorDiagnose")
            && qsetSystemInit->contains("PortErrorDiagnose")
            && qsetSystemInit->contains("PortAtmos")
            && qsetSystemInit->contains("IPAddressExchange")
            && qsetSystemInit->contains("PortExchangeGXTC")
            && qsetSystemInit->contains("PortExchangeGDCL"))
        {
            m_SCommNetSettings.qstrPortDisplay = qsetSystemInit->value("PortCommDisplay").toString();
            m_SCommNetSettings.qstrPortExposure = qsetSystemInit->value("PortCommExposure").toString();
            m_SCommNetSettings.qstrPortMasterControl = qsetSystemInit->value("PortCommMasterControl").toString();
            m_SCommNetSettings.qstrPortServo = qsetSystemInit->value("PortCommServo").toString();
            m_SCommNetSettings.qstrIPLocal = qsetSystemInit->value("IPAddressLocal").toString();
            m_SCommNetSettings.qstrIPImageTrans = qsetSystemInit->value("IPAddressImageTrans").toString();
            m_SCommNetSettings.iPortImageTrans = qsetSystemInit->value("PortImageTrans").toInt();
            m_SCommNetSettings.qstrIPMasterControl= qsetSystemInit->value("IPAddressMasterControl").toString();
            m_SCommNetSettings.iPortMasterControl = qsetSystemInit->value("PortMasterControl").toInt();
            m_SCommNetSettings.qstrIPErrDiag = qsetSystemInit->value("IPAddressErrorDiagnose").toString();
            m_SCommNetSettings.iPortErrDiag = qsetSystemInit->value("PortErrorDiagnose").toInt();
            m_SCommNetSettings.iPortAtmos = qsetSystemInit->value("PortAtmos").toInt();
            m_SCommNetSettings.qstrIPExchange = qsetSystemInit->value("IPAddressExchange").toString();
            m_SCommNetSettings.iPortExchangeGXTC = qsetSystemInit->value("PortExchangeGXTC").toInt();
            m_SCommNetSettings.iPortExchangeGDCL = qsetSystemInit->value("PortExchangeGDCL").toInt();
        }
//        else
//        {
//            delete qsetSystemInit;
//            return INIT_ERROR_COMMNETSETTINGS;
//        }
        qsetSystemInit->endGroup();
        /// [Path]
        qsetSystemInit->beginGroup("Path");
        if (qsetSystemInit->contains("DataStorePath")
                && qsetSystemInit->contains("PythonExEPath")
                && qsetSystemInit->contains("PyFilesPath"))
        {
            m_SPath.qstrDataStorePath = qsetSystemInit->value("DataStorePath").toString();
            m_STrackParams.qstrExEPath = qsetSystemInit->value("PythonExEPath").toString();
            m_STrackParams.qstrPYPath = qsetSystemInit->value("PyFilesPath").toString();
        }
        else
        {
            delete qsetSystemInit;
            return INIT_ERROR_PATH;
        }
        qsetSystemInit->endGroup();
        /// [ObsParams]
        qsetSystemInit->beginGroup("ObsParams");
        if (qsetSystemInit->contains("ObsID")
                && qsetSystemInit->contains("Longitude")
                && qsetSystemInit->contains("EW")
                && qsetSystemInit->contains("Latitude")
                && qsetSystemInit->contains("NS")
                && qsetSystemInit->contains("Altitude"))
        {
            m_SObsParams.iObsID =  qsetSystemInit->value("ObsID").toFloat();
            m_SObsParams.fLongitude = qsetSystemInit->value("Longitude").toFloat();
            m_SObsParams.bEW = !(qsetSystemInit->value("EW").toString() == "W");
            m_SObsParams.fLatitude = qsetSystemInit->value("Latitude").toFloat();
            m_SObsParams.bNS = !(qsetSystemInit->value("NS").toString() == "S");
            m_SObsParams.fAltitude = qsetSystemInit->value("Altitude").toFloat();
        }
        else
        {
            delete qsetSystemInit;
            return INIT_ERROR_PATH;
        }
        qsetSystemInit->endGroup();
        /// [DebugEN]
        qsetSystemInit->beginGroup("Debug");
        if (qsetSystemInit->contains("DebugEN")
                && qsetSystemInit->contains("DebugBW"))
        {
            m_bDebugEN = qsetSystemInit->value("DebugEN").toBool();
            m_bDebugBW = qsetSystemInit->value("DebugBW").toBool();
        }
        else
        {
            delete qsetSystemInit;
            return INIT_ERROR_PATH;
        }
        qsetSystemInit->endGroup();
        /// [ImageFormat]
        qsetSystemInit->beginGroup("ImageFormat");
        if (qsetSystemInit->contains("ImageFormat"))
        {
            m_qstrImageFormat = qsetSystemInit->value("ImageFormat").toString();
        }
        else
        {
            delete qsetSystemInit;
            return INIT_ERROR_PATH;
        }
        qsetSystemInit->endGroup();
        /// [RotateDeg]
        qsetSystemInit->beginGroup("RotateDeg");
        if (qsetSystemInit->contains("RotateDeg"))
        {
            m_SOpticData.fRotateDeg = qsetSystemInit->value("RotateDeg").toFloat();
        }
        else
        {
            delete qsetSystemInit;
            return INIT_ERROR_PATH;
        }
        qsetSystemInit->endGroup();
    }
    else
    {
        delete qsetSystemInit;
        return INIT_ERROR_FILEINVALID;
    }

    delete qsetSystemInit;
    return INIT_OK;
}

int GlobalParameter::WriteInitFile(void)
{
//    /// SystemInit.ini
//    QSettings* qsetSystemInit = new QSettings(m_SPath.qstrInitFileName, QSettings::IniFormat);
//    delete qsetSystemInit;
    return INIT_OK;
}
