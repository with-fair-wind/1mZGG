#include "UI_DispPad.h"
#include "ui_UI_DispPad.h"

UI_DispPad::UI_DispPad(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::UI_DispPad)
{
    m_pGParam = GlobalParameter::GetInstance();
    m_pImageProcessor = (ImageProcessor*)m_pGParam->m_SImageProcessorData.pvoidImageProcessor;
//    m_pDataProcessor = (DataProcessor*)m_pGParam->m_SDataProcessorData.pvoidDataProcessor;
    m_pGrabber = (Grabber*)m_pGParam->m_SGrabberData.pvoidGrabber;
    setWindowFlags(Qt::CustomizeWindowHint | Qt::WindowMinimizeButtonHint);
    setMinimumSize(500, 500);
    ui->setupUi(this);
//    setFixedSize(this->width(), this->height());
    setGeometry(230, 40, this->width(), this->height());
//    d_size=21.3;
    m_qptLUinImage = QPointF(0,0);
    m_qptRDinImage = QPointF(m_pGParam->m_SGrabberData.iFullWidth-1,m_pGParam->m_SGrabberData.iFullHeight-1);
    m_qptRightClickinImage = QPointF(m_pGParam->m_SGrabberData.iFullWidth/2,m_pGParam->m_SGrabberData.iFullHeight/2);
    m_uiZoom = 0;
    m_uiZoomCtrl = 0;
    m_uiImageWidth = 0;
    m_uiImageHeight = 0;
    m_bDrawStarMap = false;

//    m_side = 480;
    m_side = ui->QImage->width();
//    d_size = m_bHeightLonger ? m_side / (m_qptRDinImage.x() - m_qptLUinImage.x() + 1) * 100 : m_side / (m_qptRDinImage.y() - m_qptLUinImage.y() + 1) * 100;
    connect(ui->QImage, SIGNAL(LabelMouseMove(QPoint)), this, SLOT(on_LabelMouseMove(QPoint)));
    connect(ui->QImage, SIGNAL(LabelMouseDoubleClicked(QPoint)), this, SLOT(on_LabelMouseDoubleClicked(QPoint)));
    connect(ui->QImage, SIGNAL(LabelMouseClicked(QPoint)), this, SLOT(on_LabelMouseClicked(QPoint)));
    connect(ui->QImage, SIGNAL(LabelRightDoubleClicked(QPoint)), this, SLOT(on_LabelRightDoubleClicked(QPoint)));
    paintImage();
    m_bCropDisp = false;
}
#include <QDebug>
UI_DispPad::~UI_DispPad()
{
    delete ui;

    qDebug() << "UI_DispPad";
}

void UI_DispPad::keyPressEvent(QKeyEvent* event)
{
    switch (event->key())
    {
        case Qt::Key_Escape:
            event->ignore();
            break;
        case Qt::Key_Enter:
            event->ignore();
            break;
        case Qt::Key_Space:
            event->ignore();
            break;
        default:
            QDialog::keyPressEvent(event);
    }
}

void UI_DispPad::resizeEvent(QResizeEvent *event)
{
    qDebug() << "resizeEvent";
    QSize size = ui->QImage->size();

    int iSlow = size.width() < size.height() ? size.width() : size.height();
    int iLong = iSlow == size.width() ? size.height() : size.width();
    m_side = iSlow;
    if(iSlow * m_dRatio > iLong)
        m_side = iLong / m_dRatio;
    d_size = (double)m_side / (m_bHeightLonger ? (m_qptRDinImage.x() - m_qptLUinImage.x() + 1) : (m_qptRDinImage.y() - m_qptLUinImage.y() + 1)) * 100.0;

    paintImage();
    QDialog::resizeEvent(event);
}

//bool UI_DispPad::event(QEvent *event)
//{
//    if (event->type() == QEvent::Resize)
//    {
//        qDebug() << "resizeEvent";
//        QSize size = ui->QImage->size();
//        if (size.width() != size.height())
//        {
//            int side = qMin(size.width(), size.height());
//            d_size = (double)side / (double)m_uiImageWidth * 100.0;
//            paintImage();
//        }
//        return QDialog::event(event);
//    }
//    return QDialog::event(event);
//}

void UI_DispPad::paintImage()
{
    unsigned int uiWidth, uiHeight;
    unsigned char* paucDispImage = m_pImageProcessor->GetDispImage(uiWidth, uiHeight);
    bool bImageSizeChanged = (m_uiImageWidth != uiWidth) || (m_uiImageHeight != uiHeight);
    if (bImageSizeChanged)  // Clear Display Pad
    {
        m_uiImageWidth = uiWidth;
        m_uiImageHeight = uiHeight;
        paucDispImage = new unsigned char[m_uiImageWidth * m_uiImageHeight];
        memset(paucDispImage, 0, m_uiImageWidth * m_uiImageHeight);

        m_bHeightLonger = (m_uiImageHeight > m_uiImageWidth);
        m_dRatio = m_bHeightLonger ? (double)m_uiImageHeight / (double)m_uiImageWidth : (double)m_uiImageWidth / (double)m_uiImageHeight;

        if (m_uiImageWidth == m_pGParam->m_SGrabberData.iFullWidth && m_uiImageHeight == m_pGParam->m_SGrabberData.iFullHeight)
        {
            d_size = m_uiImageWidth < m_uiImageHeight ? (double)m_side/(double)m_uiImageWidth * 100.0 : (double)m_side/(double)m_uiImageHeight * 100.0;
            m_qptLUinImage = QPointF(0,0);
            m_qptRDinImage = QPointF(m_pGParam->m_SGrabberData.iFullWidth-1,m_pGParam->m_SGrabberData.iFullHeight-1);
            m_qptRightClickinImage = QPointF(m_pGParam->m_SGrabberData.iFullWidth/2,m_pGParam->m_SGrabberData.iFullHeight/2);
        }
        else if (m_uiImageWidth == m_pGParam->m_SGrabberData.iSubWidth && m_uiImageHeight == m_pGParam->m_SGrabberData.iSubHeight)
        {
            d_size = m_uiImageWidth < m_uiImageHeight ? (double)m_side/(double)m_uiImageWidth * 100.0 : (double)m_side/(double)m_uiImageHeight * 100.0;
            m_qptLUinImage = QPointF(0,0);
            m_qptRDinImage = QPointF(m_pGParam->m_SGrabberData.iSubWidth-1,m_pGParam->m_SGrabberData.iSubHeight-1);
            m_qptRightClickinImage = QPointF(m_pGParam->m_SGrabberData.iSubWidth/2,m_pGParam->m_SGrabberData.iSubHeight/2);
        }
    }

    double dSizePercent = d_size / 100.0;

    if (paucDispImage)
    {
        QImage8UC1BufferPackage qimgDisp(paucDispImage, uiWidth, uiHeight);
        QImage qimgCrop = qimgDisp.copy((int)m_qptLUinImage.x(), (int)m_qptLUinImage.y(),
                                        (int)(m_qptRDinImage.x() - m_qptLUinImage.x() + 1), (int)(m_qptRDinImage.y()-m_qptLUinImage.y() + 1));
        m_pqimgScale = qimgCrop.scaled(qimgCrop.width()*dSizePercent,qimgCrop.height()*dSizePercent);

        QPixmap qpixmapDraw = QPixmap::fromImage(m_pqimgScale);
        QPainter* pqpainterDraw = new QPainter;
        pqpainterDraw->begin(&qpixmapDraw);

        int iProcMode = m_pImageProcessor->GetProcMode();
        int iTrackMode = m_pImageProcessor->GetTrackMode();
        if ((iProcMode == PROC_DIRECT && iTrackMode == TRACK_LEO)
          || (iProcMode == PROC_DIRECT && iTrackMode == TRACK_SC))
        {
            sTargetInfo info;
            vector<sTargetInfo> vectInfo;
            m_pImageProcessor->GetVectTargetTWDWInfo(vectInfo);
            if(!vectInfo.empty())
                info = vectInfo[0];
            if (info.vectInfoInFrame.size() > 3 && info.bLiving)
            {
                sMeasureBlob blob = info.vectInfoInFrame[info.vectInfoInFrame.size()-1].blobMeasure;

                float fRectLeft, fRectWidth, fRectUp, fRectHeight;
//                if (blob.fMaxX - blob.fMinX < 20
//                        && blob.fMaxY - blob.fMinY < 20)
                {
                    fRectLeft = blob.pairfPos.first - 30;
                    fRectWidth = 61;
                    fRectUp = blob.pairfPos.second - 30;
                    fRectHeight = 61;
                }
//                else
//                {
//                    fRectLeft = blob.fMinX-10;
//                    fRectWidth = blob.fMaxX-blob.fMinX+20;
//                    fRectUp = blob.fMinY-10;
//                    fRectHeight = blob.fMaxY-blob.fMinY+20;
//                }

                pqpainterDraw->setPen(QPen(Qt::green, 2, Qt::SolidLine));
                pqpainterDraw->drawRect(QRect(fRectLeft * dSizePercent, fRectUp * dSizePercent, fRectWidth * dSizePercent, fRectHeight * dSizePercent));
                pqpainterDraw->setPen(QPen(Qt::green, 1, Qt::SolidLine));
                QFont qfontMag = pqpainterDraw->font();
                qfontMag.setPixelSize(18);
                pqpainterDraw->setFont(qfontMag);
                if (iTrackMode != TRACK_SC)
                {
                    pqpainterDraw->drawText((int)((fRectLeft) * dSizePercent),
                                            (int)((fRectUp-10) * dSizePercent),
                                            info.qstrTargetID);
                }
                else
                {
                    pqpainterDraw->drawText((int)((fRectLeft) * dSizePercent),
                                            (int)((fRectUp-10) * dSizePercent),
                                            "Calib Star");
                }

                pqpainterDraw->setPen(QPen(Qt::yellow, 1, Qt::SolidLine));
                QFont qfontDraw = pqpainterDraw->font();
                qfontDraw.setPixelSize(18);
                pqpainterDraw->setFont(qfontDraw);
                QString qstrDistX, qstrDistY;
                qstrDistX.sprintf("Dx: %.1f", blob.pairfPos.first - m_pGParam->m_SOpticData.fOptCenterX);
                qstrDistY.sprintf("Dy: %.1f", m_pGParam->m_SOpticData.fOptCenterY - blob.pairfPos.second);
                pqpainterDraw->drawText(10, 270, qstrDistX);
                pqpainterDraw->drawText(150, 270, qstrDistY);
                QString qstrDN;
                float fRatio = blob.fArea / 10 * 1.7396483216;
                qstrDN.sprintf("DN: %.1f", blob.fArea * fRatio);
                pqpainterDraw->drawText(10, 300, qstrDN);
            }
        }
        else if ((iProcMode == PROC_DIFF && iTrackMode == TRACK_GEO)
                 || (iProcMode == PROC_DIRECT && iTrackMode == TRACK_GEO))
        {
            vector<sTargetInfo> vectInfo;
            m_pImageProcessor->GetVectTargetTWDWInfo(vectInfo);
            int iNumFrames = m_pImageProcessor->GetNumFrames();
            if (iNumFrames >= 4)
            {
                for (int i = 0; i < vectInfo.size(); i++)
                {
                    sTargetInfo info = vectInfo[i];
                    if (info.bLiving)
                    {
                        sTargetInfoInFrame infoInFrame;
                        infoInFrame = info.vectInfoInFrame[info.vectInfoInFrame.size()-1];

                        sMeasureBlob blob = infoInFrame.blobMeasure;
                        float fRectLeft, fRectWidth, fRectUp, fRectHeight;
                        if (info.vectInfoInFrame[info.vectInfoInFrame.size()-1].bValid
                                || info.vectInfoInFrame[info.vectInfoInFrame.size()-2].bValid
                                || info.vectInfoInFrame[info.vectInfoInFrame.size()-3].bValid)
                        {
                            fRectLeft = blob.fMinX-m_qptLUinImage.x()-30;
                            fRectWidth = blob.fMaxX-blob.fMinX+60;
                            fRectUp = blob.fMinY-m_qptLUinImage.y()-30;
                            fRectHeight = blob.fMaxY-blob.fMinY+60;

                            pqpainterDraw->setPen(QPen(Qt::green, 2, Qt::SolidLine));
                            pqpainterDraw->drawRect(QRect(fRectLeft * dSizePercent, fRectUp * dSizePercent, fRectWidth * dSizePercent, fRectHeight * dSizePercent));
                            pqpainterDraw->setPen(QPen(Qt::green, 1, Qt::SolidLine));
                            QFont qfontMag = pqpainterDraw->font();
                            qfontMag.setPixelSize(18);
                            pqpainterDraw->setFont(qfontMag);
                            pqpainterDraw->drawText((int)((fRectLeft) * dSizePercent),
                                                    (int)((fRectUp-10) * dSizePercent),
                                                    info.qstrTargetID);
                        }
                    }
                }
            }            
        }
        else if (iProcMode == PROC_DIRECT && iTrackMode == TRACK_MANUAL)
        {
            sTargetInfo info;
            vector<sTargetInfo> vectInfo;
            m_pImageProcessor->GetVectTargetTWDWInfo(vectInfo);
            if(!vectInfo.empty())
                info = vectInfo[0];
            if (info.vectInfoInFrame.size() > 3 && info.bLiving)
            {
                sMeasureBlob blob = info.vectInfoInFrame[info.vectInfoInFrame.size()-1].blobMeasure;

                float fRectLeft, fRectWidth, fRectUp, fRectHeight;
                fRectLeft = blob.fMinX-m_qptLUinImage.x()-30;
                fRectWidth = blob.fMaxX-blob.fMinX+60;
                fRectUp = blob.fMinY-m_qptLUinImage.y()-30;
                fRectHeight = blob.fMaxY-blob.fMinY+60;

                pqpainterDraw->setPen(QPen(Qt::green, 2, Qt::SolidLine));
                pqpainterDraw->drawRect(QRect(fRectLeft * dSizePercent, fRectUp * dSizePercent, fRectWidth * dSizePercent, fRectHeight * dSizePercent));
                pqpainterDraw->setPen(QPen(Qt::green, 1, Qt::SolidLine));
                QFont qfontMag = pqpainterDraw->font();
                qfontMag.setPixelSize(18);
                pqpainterDraw->setFont(qfontMag);
                pqpainterDraw->drawText((int)((fRectLeft) * dSizePercent),
                                        (int)((fRectUp-10) * dSizePercent),
                                        info.qstrTargetID);
            }
        }

        if (m_bDrawStarMap)
        {
            if (!m_pGParam->m_SGrabberData.bWindowEN)
            {
                /// 画理论星图
                int nYear, nMonth, nDay, nHour;
                m_pImageProcessor->BJ2UTC(m_pGParam->m_SImageProcessorData.sexpdispdataDisp.iYMDYear,
                       m_pGParam->m_SImageProcessorData.sexpdispdataDisp.iYMDMonth,
                       m_pGParam->m_SImageProcessorData.sexpdispdataDisp.iYMDDay,
                       m_pGParam->m_SImageProcessorData.sexpdispdataDisp.iHourRecv,
                       nYear, nMonth, nDay, nHour);
                double dblSecond=m_pGParam->m_SImageProcessorData.sexpdispdataDisp.iSecondRecv + m_pGParam->m_SImageProcessorData.sexpdispdataDisp.iMilliSecondRecv * 0.001 + m_pGParam->m_SImageProcessorData.sexpdispdataDisp.iMicroSecondRecv * 0.000001;
                double dblHour=nHour + m_pGParam->m_SImageProcessorData.sexpdispdataDisp.iMinuteRecv/60e0+dblSecond/3600e0;
                double dblPointA=m_pGParam->m_SImageProcessorData.sexpdispdataDisp.dAzimuthDegreeTotalRecv;
                double dblPointE=m_pGParam->m_SImageProcessorData.sexpdispdataDisp.dElevationDegreeTotalRecv;
                double dAziErr = 0;
                double dEleErr = 0;
                m_pImageProcessor->CalcPointError(dblPointA, dblPointE, dAziErr, dEleErr);
                dblPointA -= dAziErr;
                dblPointE -= dEleErr;
                m_pImageProcessor->GetStarMap()->StarSearch_DJ_AP(nYear, nMonth, nDay,
                                                            dblHour/* + 2.0/3600e0*/,
                                                            dblPointA, dblPointE,
                                                            m_pGParam->m_SOpticData.fPixelScale * m_pGParam->m_SGrabberData.iFullWidth, m_pGParam->m_SOpticData.fPixelScale * m_pGParam->m_SGrabberData.iFullHeight,
                                                            0, m_pGParam->m_sNetAtmosData.fTemp+273.15, m_pGParam->m_sNetAtmosData.fAtmosP/100.0, m_pGParam->m_sNetAtmosData.fHumidity, 1);
                vector<pair<pair<float, float>, float>> vectStarDraw;
                for(int i = 0;i < m_pImageProcessor->GetStarMap()->nSeekStar; i++)
                {
                   double dPosX, dPosY, dMag;
                   dPosX = m_pImageProcessor->GetStarMap()->dblSeekStar[i][3];
                   dPosY = m_pImageProcessor->GetStarMap()->dblSeekStar[i][4];
                   if (m_pGParam->m_SImageProcessorData.bFullLEO)
                   {
                       dPosX *= 2.0;
                       dPosY *= 2.0;
                   }
                   dMag = m_pImageProcessor->GetStarMap()->dblSeekStar[i][2];
                   double dJBX, dJBY;
                   m_pImageProcessor->CalcDistortionDelta(dPosX, dPosY, dJBX, dJBY);
                   dPosX += dJBX;
                   dPosY += dJBY;
                   pair<pair<float, float>, float> StarDraw;
                   StarDraw = pair<pair<float, float>, float>(pair<float, float>(dPosX-m_qptLUinImage.x(), dPosY-m_qptLUinImage.y()), dMag);
                   vectStarDraw.push_back(StarDraw);
                }
                pqpainterDraw->setPen(QPen(Qt::yellow, 1, Qt::SolidLine));
                pqpainterDraw->setBrush(Qt::yellow);
                for (int i = 0; i < vectStarDraw.size(); i++)
                {
                    pqpainterDraw->drawEllipse(QPoint((int)(vectStarDraw[i].first.first * dSizePercent), (int)(vectStarDraw[i].first.second * dSizePercent)),
                                               (int)(5 * dSizePercent), (int)(5 * dSizePercent));                    
                    QFont qfontMag = pqpainterDraw->font();
                    qfontMag.setPixelSize(12);
                    pqpainterDraw->setFont(qfontMag);
                    pqpainterDraw->drawText((int)((vectStarDraw[i].first.first-6) * dSizePercent),
                                            (int)((vectStarDraw[i].first.second-6) * dSizePercent),
                                            QString::number(vectStarDraw[i].second, 'f', 2));
                }
//                /// 画实际星图
//                vector<pair<float, float>> vectpairfStarPos;
//                m_pDataProcessor->GetCalcStarPos(vectpairfStarPos);
//                pqpainterDraw->setPen(QPen(Qt::blue, 1, Qt::SolidLine));
//                pqpainterDraw->setBrush(Qt::blue);
//                for (int i = 0; i < vectpairfStarPos.size(); i++)
//                {
//                    pqpainterDraw->drawEllipse(QPoint((int)(vectpairfStarPos[i].first * dSizePercent), (int)(vectpairfStarPos[i].second * dSizePercent)),
//                                               (int)(5 * dSizePercent), (int)(5 * dSizePercent));
//                }
//                m_pDataProcessor->ClearCalcStarPos();

//                /// 画理论星图
//                int nYear, nMonth, nDay, nHour;

//                m_pDataProcessor->BJ2UTC(2021,
//                       3,m_qptRDinImage
//                       4,
//                       23,
//                       nYear, nMonth, nDay, nHour);
//                double dblSecond = 33 + 538 * 0.001 + 0 * 0.000001;
//                double dblHour = nHour + 53/60e0 + dblSecond/3600e0;
//                double dblPointA = 181 + 48/60e0 + 26/3600e0;
//                double dblPointE = 38 + 27/60e0 + 7/3600e0;

//                m_pDataProcessor->GetStarMap()->StarSearch_DJ_AP(nYear, nMonth, nDay,
//                                                            dblHour,
//                                                            dblPointA, dblPointE,
//                                                            m_pGParam->m_SOpticData.fPixelScale * m_pGParam->m_SGrabberData.iFullWidth, m_pGParam->m_SOpticData.fPixelScale * m_pGParam->m_SGrabberData.iFullHeight,
//                                                            0, m_pGParam->m_sNetAtmosData.fTemp+273.15, m_pGParam->m_sNetAtmosData.fAtmosP/100.0, m_pGParam->m_sNetAtmosData.fHumidity, 1);
//                vector<pair<pair<float, float>, float>> vectStarDraw;
//                for(int i = 0;i < m_pDataProcessor->GetStarMap()->nSeekStar; i++)
//                {
//                   double dPosX, dPosY, dMag;
//                   dPosX = m_pDataProcessor->GetStarMap()->dblSeekStar[i][3];
//                   dPosY = m_pDataProcessor->GetStarMap()->dblSeekStar[i][4];
//                   dMag = m_pDataProcessor->GetStarMap()->dblSeekStar[i][2];
//                   double dAlpha = m_pDataProcessor->GetStarMap()->dblSeekStar[i][0];
//                   double dDelta = m_pDataProcessor->GetStarMap()->dblSeekStar[i][1];
//                   double dAzi, dEle;
//                   m_pDataProcessor->GetStarMap()->StarAop(dAlpha/180.0*3.1415926,
//                                                                           dDelta/180.0*3.1415926,
//                                                                           nYear, nMonth, nDay, dblHour, 0,
//                                                                           m_pGParam->m_sNetAtmosData.fTemp+273.15, m_pGParam->m_sNetAtmosData.fAtmosP/100.0, m_pGParam->m_sNetAtmosData.fHumidity,
//                                                                           &dAzi, &dEle);
//                   double dJBX, dJBY;
//                   if (abs(dMag - 3.84) < 0.01)
//                   {
//                        int a = 0;
//                        a++;
//                   }
//                   m_pDataProcessor->CalcDistortionDelta(dPosX, dPosY, dJBX, dJBY);
//                   dPosX += dJBX;
//                   dPosY += dJBY;
//                   if (abs(dMag - 3.84) < 0.01)
//                   {
//                        int a = 0;
//                        a++;
//                   }
//                   pair<pair<float, float>, float> StarDraw;
//                   StarDraw = pair<pair<float, float>, float>(pair<float, float>(dPosX, dPosY), dMag);
//                   vectStarDraw.push_back(StarDraw);
//                }
//                pqpainterDraw->setPen(QPen(Qt::yellow, 1, Qt::SolidLine));
//                pqpainterDraw->setBrush(Qt::yellow);
//                for (int i = 0; i < vectStarDraw.size(); i++)
//                {
//                    pqpainterDraw->drawEllipse(QPoint((int)(vectStarDraw[i].first.first * dSizePercent), (int)(vectStarDraw[i].first.second * dSizePercent)),
//                                               (int)(5 * dSizePercent), (int)(5 * dSizePercent));
//                    QFont qfontMag = pqpainterDraw->font();
//                    qfontMag.setPixelSize(12);
//                    pqpainterDraw->setFont(qfontMag);
//                    pqpainterDraw->drawText((int)((vectStarDraw[i].first.first-6) * dSizePercent),
//                                            (int)((vectStarDraw[i].first.second-6) * dSizePercent),
//                                            QString::number(vectStarDraw[i].second, 'f', 2));
//                }
            }
            else
            {
                int nYear, nMonth, nDay, nHour;
                m_pImageProcessor->BJ2UTC(m_pGParam->m_SImageProcessorData.sexpdispdataDisp.iYMDYear,
                       m_pGParam->m_SImageProcessorData.sexpdispdataDisp.iYMDMonth,
                       m_pGParam->m_SImageProcessorData.sexpdispdataDisp.iYMDDay,
                       m_pGParam->m_SImageProcessorData.sexpdispdataDisp.iHourRecv,
                       nYear, nMonth, nDay, nHour);
                double dblSecond=m_pGParam->m_SImageProcessorData.sexpdispdataDisp.iSecondRecv + m_pGParam->m_SImageProcessorData.sexpdispdataDisp.iMilliSecondRecv * 0.001 + m_pGParam->m_SImageProcessorData.sexpdispdataDisp.iMicroSecondRecv * 0.000001;
                double dblHour=nHour + m_pGParam->m_SImageProcessorData.sexpdispdataDisp.iMinuteRecv/60e0+dblSecond/3600e0;
                double dblPointA=m_pGParam->m_SImageProcessorData.sexpdispdataDisp.dAzimuthDegreeTotalRecv;
                double dblPointE=m_pGParam->m_SImageProcessorData.sexpdispdataDisp.dElevationDegreeTotalRecv;
                double dAziErr = 0;
                double dEleErr = 0;
                m_pImageProcessor->CalcPointError(dblPointA, dblPointE, dAziErr, dEleErr);
                dblPointA -= dAziErr;
                dblPointE -= dEleErr;
                m_pImageProcessor->GetStarMap()->StarSearch_DJ_AP(nYear, nMonth, nDay,
                                                            dblHour,
                                                            dblPointA, dblPointE,
                                                            m_pGParam->m_SOpticData.fPixelScale * m_pGParam->m_SGrabberData.iSubWidth, m_pGParam->m_SOpticData.fPixelScale * m_pGParam->m_SGrabberData.iSubHeight,
                                                            0, m_pGParam->m_sNetAtmosData.fTemp+273.15, m_pGParam->m_sNetAtmosData.fAtmosP/100.0, m_pGParam->m_sNetAtmosData.fHumidity, 1);
                vector<pair<pair<float, float>, float>> vectStarDraw;
                for(int i = 0;i < m_pImageProcessor->GetStarMap()->nSeekStar; i++)
                {             
                   double dPosX, dPosY, dMag;
                   dPosX = m_pImageProcessor->GetStarMap()->dblSeekStar[i][3];
                   dPosY = m_pImageProcessor->GetStarMap()->dblSeekStar[i][4];
                   dMag = m_pImageProcessor->GetStarMap()->dblSeekStar[i][2];
                   pair<pair<float, float>, float> StarDraw;
                   StarDraw = pair<pair<float, float>, float>(pair<float, float>(dPosX, dPosY), dMag);
                   vectStarDraw.push_back(StarDraw);
                }
                pqpainterDraw->setPen(QPen(Qt::yellow, 1, Qt::SolidLine));
                pqpainterDraw->setBrush(Qt::yellow);
                for (int i = 0; i < vectStarDraw.size(); i++)
                {
                    pqpainterDraw->drawEllipse(QPoint((int)(vectStarDraw[i].first.first * dSizePercent), (int)(vectStarDraw[i].first.second * dSizePercent)),
                                               (int)(5 * dSizePercent), (int)(5 * dSizePercent));
                    QFont qfontMag = pqpainterDraw->font();
                    qfontMag.setPixelSize(12);
                    pqpainterDraw->setFont(qfontMag);
                    pqpainterDraw->drawText((int)((vectStarDraw[i].first.first-6) * dSizePercent),
                                            (int)((vectStarDraw[i].first.second-6) * dSizePercent),
                                            QString::number(vectStarDraw[i].second, 'f', 2));
                }
            }
        }

        pqpainterDraw->setPen(QPen(Qt::yellow, 1, Qt::SolidLine));
        QFont qfontDraw = pqpainterDraw->font();
        qfontDraw.setPixelSize(18);
        pqpainterDraw->setFont(qfontDraw);

        QString qstrDate, qstrTime, qstrA, qstrE, qstrExpTime, qstrCam, qstrAtmos;
        qstrDate.sprintf("%04d - %02d - %02d",m_pGParam->m_SImageProcessorData.sexpdispdataDisp.iYMDYear,
                         m_pGParam->m_SImageProcessorData.sexpdispdataDisp.iYMDMonth,
                         m_pGParam->m_SImageProcessorData.sexpdispdataDisp.iYMDDay);
        qstrTime.sprintf("%02d : %02d : %02d . %03d", m_pGParam->m_SImageProcessorData.sexpdispdataDisp.iHourRecv,
                         m_pGParam->m_SImageProcessorData.sexpdispdataDisp.iMinuteRecv,
                         m_pGParam->m_SImageProcessorData.sexpdispdataDisp.iSecondRecv,
                         m_pGParam->m_SImageProcessorData.sexpdispdataDisp.iMilliSecondRecv);
        qstrA.sprintf("A:  %.4f °", m_pGParam->m_SImageProcessorData.sexpdispdataDisp.dAzimuthDegreeTotalRecv);
        qstrE.sprintf("E:  %.4f °", m_pGParam->m_SImageProcessorData.sexpdispdataDisp.dElevationDegreeTotalRecv);
        double dExpTime = m_pImageProcessor->GetProcessMode() ? (m_pGrabber->GetGrabStatus() ? m_pGParam->m_SGrabberData.dExposureTime : 0) : (m_pGParam->m_SImageReplayerData.dExposureTimeDisp);
        qstrExpTime.sprintf("Exp:  %.1f ms", dExpTime);
//        qstrCam.sprintf("Cam:  %.1f H,   %.1f ℃", m_pGrabber->GetCamHours(), m_pGrabber->GetCamTemp());
        qstrCam.sprintf("Cam:  %.1f H", m_pGrabber->GetCamHours());
        qstrAtmos.sprintf("Env:  %.1f ℃,   %.1f %%,   %.1f hPa",
                          m_pGParam->m_SImageProcessorData.sexpdispdataDisp.fTemp,
                          m_pGParam->m_SImageProcessorData.sexpdispdataDisp.fHumidity*100.0,
                          m_pGParam->m_SImageProcessorData.sexpdispdataDisp.fAtmosP/100.0);
        pqpainterDraw->drawText(10, 30, qstrDate);
        pqpainterDraw->drawText(150, 30, qstrTime);
        pqpainterDraw->drawText(10, 60, qstrA);
        pqpainterDraw->drawText(150, 60, qstrE);
        pqpainterDraw->drawText(10, 90, qstrExpTime);
        pqpainterDraw->drawText(10, 120, qstrCam);
        pqpainterDraw->drawText(10, 150, qstrAtmos);

        QString qstrProc;
        qstrProc.sprintf("Proc:  Avr=%.1f,  Std=%.1f,  Ratio=%.1f,  Meas=%d,  Time=%dms",
                         m_pGParam->m_SImageProcessorData.fAvr,
                         m_pGParam->m_SImageProcessorData.fStd,
                         m_pGParam->m_SImageProcessorData.fRatioStd,
                         m_pGParam->m_SImageProcessorData.uiNumMeasure,
                         m_pGParam->m_SImageProcessorData.iProcTime);
        pqpainterDraw->drawText(10, 180, qstrProc);

        QString qstrTrack;
        qstrTrack.sprintf("Track:  Star=(%d,%d),  Match=%d,  RadiusS=%d,  RadiusT=%d, Thresh=%.1f,  Time=%dms",
                         (unsigned int)m_pGParam->m_SImageProcessorData.pairfStarSpd.first,
                         (unsigned int)m_pGParam->m_SImageProcessorData.pairfStarSpd.second,
                         m_pGParam->m_SImageProcessorData.uiNumStarMatch,
                         (int)m_pGParam->m_SImageProcessorData.fRadiusS,
                          (int)m_pGParam->m_SImageProcessorData.fRadiusT,
                         m_pGParam->m_SImageProcessorData.fThreshErr,
                         m_pGParam->m_SImageProcessorData.uiProcTrackMs);
        pqpainterDraw->drawText(10, 210, qstrTrack);

        QString qstrErrTWDW;
        qstrErrTWDW.sprintf("Astro:  X=%.1fp,  Y=%.1fp,  R=%.4f°,  Match=%d,  Reset=%d,  Calc=%d,  Time=%dms",
                         m_pGParam->m_SDataProcessorData.bReady ? m_pGParam->m_SDataProcessorData.fErrAzi : 0,
                         m_pGParam->m_SDataProcessorData.bReady ? m_pGParam->m_SDataProcessorData.fErrEle : 0,
                         m_pGParam->m_SDataProcessorData.bReady ? m_pGParam->m_SDataProcessorData.fRotate : 0,
                         m_pGParam->m_SDataProcessorData.bReady ? m_pGParam->m_SDataProcessorData.iNumStarMatched : 0,
                         m_pGParam->m_SDataProcessorData.bReady ? (m_pGParam->m_SDataProcessorData.bValid ? 1 : 0) : 0,
                         m_pGParam->m_SDataProcessorData.iNumStarCalc,
                         m_pGParam->m_SDataProcessorData.iTWDWTime);
        m_pGParam->m_SDataProcessorData.bReady = false;
        pqpainterDraw->drawText(10, 240, qstrErrTWDW);

        if (true)
        {
            double dOptCenterX = m_pGParam->m_SOpticData.fOptCenterX - m_qptLUinImage.x();
            double dOptCenterY = m_pGParam->m_SOpticData.fOptCenterY - m_qptLUinImage.y();
            double dRotate = 0;
            double dROuter = uiWidth * dSizePercent * 0.3;
            double dRInner = uiHeight * dSizePercent * 0.02;
            QLineF qlineLeft((-dROuter * cos(dRotate / 180.0 * 3.1415926) + dOptCenterX * dSizePercent) ,
                             (dROuter * sin(dRotate / 180.0 * 3.1415926) + dOptCenterY * dSizePercent) ,
                             (-dRInner * cos(dRotate / 180.0 * 3.1415926) + dOptCenterX * dSizePercent),
                             (dRInner * sin(dRotate / 180.0 * 3.1415926) + dOptCenterY * dSizePercent));
            QLineF qlineRight((dROuter * cos(dRotate / 180.0 * 3.1415926) + dOptCenterX * dSizePercent),
                              (-dROuter * sin(dRotate / 180.0 * 3.1415926) + dOptCenterY * dSizePercent),
                              (dRInner * cos(dRotate / 180.0 * 3.1415926) + dOptCenterX * dSizePercent),
                              (-dRInner * sin(dRotate / 180.0 * 3.1415926) + dOptCenterY * dSizePercent));
            QLineF qlineUp((-dROuter * sin(dRotate / 180.0 * 3.1415926) + dOptCenterX * dSizePercent),
                           (-dROuter * cos(dRotate / 180.0 * 3.1415926) + dOptCenterY * dSizePercent),
                           (-dRInner * sin(dRotate / 180.0 * 3.1415926) + dOptCenterX * dSizePercent),
                           (-dRInner * cos(dRotate / 180.0 * 3.1415926) + dOptCenterY * dSizePercent));
            QLineF qlineDown((dROuter * sin(dRotate / 180.0 * 3.1415926) + dOptCenterX * dSizePercent),
                             (dROuter * cos(dRotate / 180.0 * 3.1415926) + dOptCenterY * dSizePercent),
                             (dRInner * sin(dRotate / 180.0 * 3.1415926) + dOptCenterX * dSizePercent),
                             (dRInner * cos(dRotate / 180.0 * 3.1415926) + dOptCenterY * dSizePercent));
            pqpainterDraw->setPen(QPen(Qt::yellow, 1, Qt::SolidLine));
            pqpainterDraw->drawLine(qlineLeft);
            pqpainterDraw->setPen(QPen(Qt::yellow, 1, Qt::DashLine));
            pqpainterDraw->drawLine(qlineRight);
            pqpainterDraw->setPen(QPen(Qt::yellow, 1, Qt::DashLine));
            pqpainterDraw->drawLine(qlineUp);
            pqpainterDraw->setPen(QPen(Qt::yellow, 1, Qt::SolidLine));
            pqpainterDraw->drawLine(qlineDown);
            pqpainterDraw->setPen(QPen(Qt::yellow, 1, Qt::SolidLine));
            pqpainterDraw->drawPoint(dOptCenterX * dSizePercent, dOptCenterY * dSizePercent);
            pqpainterDraw->end();
        }

        ui->QImage->setPixmap(qpixmapDraw);
    }
    if (bImageSizeChanged)  delete [] paucDispImage;
}

void UI_DispPad::on_SignalDisplay()
{
    paintImage();
}

void UI_DispPad::on_SignalClose()
{
    close();
}

void UI_DispPad::on_SignalDrawStarMap(bool bDrawStarMap )
{
    m_bDrawStarMap = bDrawStarMap;
    paintImage();
}

void UI_DispPad::on_LabelMouseMove(QPoint qptPos)
{
    double dSizePercent = d_size / 100.0;
//    qDebug() << "Mouse Pos: (" << qptPos.x()/dSizePercent << ", " << qptPos.y()/dSizePercent << ")." << endl;
    m_pGParam->m_SImageProcessorData.pairfMousePos = pair<float,float>(qptPos.x()/dSizePercent,qptPos.y()/dSizePercent);
}

void UI_DispPad::on_LabelMouseDoubleClicked(QPoint qptPos)
{
    double dSizePercent = d_size / 100.0;
    qDebug() << "Double Clicked Pos: (" << qptPos.x()/dSizePercent << ", " << qptPos.y()/dSizePercent << ")." << endl;
    unsigned int uiWidth, uiHeight;
    unsigned char* paucDispImage = m_pImageProcessor->GetDispImage(uiWidth, uiHeight);
    float fClickX = qptPos.x() / dSizePercent + m_qptLUinImage.x();
    float fClickY = qptPos.y() / dSizePercent + m_qptLUinImage.y();
    if (fClickX >= 50 && fClickX <= uiWidth - 1 - 50 && fClickY >= 50 && fClickY <= uiHeight - 1 - 50)
    {
        m_pGParam->m_SImageProcessorData.bDoubleClicked = true;
        m_pGParam->m_SImageProcessorData.pairfClickedPos = pair<float,float>(fClickX,fClickY);
        emit SignalManualInit();
    }
}

void UI_DispPad::on_LabelMouseClicked(QPoint qptPos)
{
    double dSizePercent = d_size / 100.0;
    float fClickX = qptPos.x() / dSizePercent + m_qptLUinImage.x();
    float fClickY = qptPos.y() / dSizePercent + m_qptLUinImage.y();
    emit SignalLabelMouseClicked(QPoint(fClickX,fClickY));
}

void UI_DispPad::on_LabelRightDoubleClicked(QPoint qptPos)
{
    if (m_uiImageWidth == m_pGParam->m_SGrabberData.iFullWidth && m_uiImageHeight == m_pGParam->m_SGrabberData.iFullHeight)
    {
        if (m_uiZoomCtrl == 0)
        {
            m_uiZoom = 0;
        }
        else if (m_uiZoomCtrl == 1)
        {
            if (++m_uiZoom > 2)
                m_uiZoom = 2;
        }
        else if (m_uiZoomCtrl == 2)
        {
            if (--m_uiZoom < 0)
                m_uiZoom = 0;
        }

        QSize size = ui->QImage->size();
        int iSlow = size.width() < size.height() ? size.width() : size.height();
        int iLong = iSlow == size.width() ? size.height() : size.width();
        m_side = iSlow;
        if(iSlow * m_dRatio > iLong)
            m_side = iLong / m_dRatio;

        switch (m_uiZoom)
        {
        case 0:
            d_size = (float)m_side/(m_bHeightLonger ? (double)m_uiImageWidth : (double)m_uiImageHeight) * 100.0;
            break;
        case 1:
            {
                double dBiggerFirst = (m_bHeightLonger ? (double)m_uiImageWidth : (double)m_uiImageHeight) * (double)BIGGER_FIRST / (double)100;
                d_size = (double)m_side / dBiggerFirst * 100.0;
                break;
            }
        case 2:
            {
                double dBiggerSecond = (m_bHeightLonger ? (double)m_uiImageWidth : (double)m_uiImageHeight) * (double)BIGGER_SECOND / (double)100;
                d_size = (double)m_side / dBiggerSecond * 100.0;
                break;
            }
        default:
            break;
        }
        if (m_uiZoom == 0)
        {
            m_qptRightClickinImage.setX(m_pGParam->m_SGrabberData.iFullWidth/2);
            m_qptRightClickinImage.setY(m_pGParam->m_SGrabberData.iFullHeight/2);
            m_qptLUinImage = QPointF(0,0);
            m_qptRDinImage = QPointF(m_pGParam->m_SGrabberData.iFullWidth-1,m_pGParam->m_SGrabberData.iFullHeight-1);
        }
        else
        {
            m_qptRightClickinImage.setX(qptPos.x() / (m_bHeightLonger ? (float)m_side : (float)m_side * m_dRatio) * (m_qptRDinImage.x() - m_qptLUinImage.x() + 1) + m_qptLUinImage.x());
            m_qptRightClickinImage.setY(qptPos.y() / (m_bHeightLonger ? (float)m_side * m_dRatio : (float)m_side) * (m_qptRDinImage.y() - m_qptLUinImage.y() + 1) + m_qptLUinImage.y());
            float fPercent = d_size / 100.0;
            float fWidth = (m_bHeightLonger ? (float)m_side : (float)m_side * m_dRatio) / fPercent;
            float fHeight = (m_bHeightLonger ? (float)m_side * m_dRatio : (float)m_side) * m_dRatio / fPercent;
            float fLeft = m_qptRightClickinImage.x() - fWidth / 2 < 0 ? 0 : m_qptRightClickinImage.x() - fWidth / 2;
            float fRight = m_qptRightClickinImage.x() + fWidth / 2 > m_pGParam->m_SGrabberData.iFullWidth-1 ? m_pGParam->m_SGrabberData.iFullWidth-1 : m_qptRightClickinImage.x() + fWidth / 2;
            float fUp = m_qptRightClickinImage.y() - fHeight / 2 < 0 ? 0 : m_qptRightClickinImage.y() - fHeight / 2;
            float fDown = m_qptRightClickinImage.y() + fHeight / 2 > m_pGParam->m_SGrabberData.iFullHeight-1 ? m_pGParam->m_SGrabberData.iFullHeight-1 : m_qptRightClickinImage.y() + fHeight / 2;
            if (fLeft == 0)
            {
                fRight = fWidth - 1;
                m_qptRightClickinImage.setX((fLeft + fRight)/2);
            }
            if (fRight == m_pGParam->m_SGrabberData.iFullWidth-1)
            {
                fLeft = m_pGParam->m_SGrabberData.iFullWidth-1 - fWidth + 1;
                m_qptRightClickinImage.setX((fLeft + fRight)/2);
            }
            if (fUp == 0)
            {
                fDown = fHeight - 1;
                m_qptRightClickinImage.setY((fLeft + fRight)/2);
            }
            if (fDown == m_pGParam->m_SGrabberData.iFullHeight-1)
            {
                fUp = m_pGParam->m_SGrabberData.iFullHeight-1 - fHeight + 1;
                m_qptRightClickinImage.setY((fLeft + fRight)/2);
            }
            m_qptLUinImage.setX(fLeft);
            m_qptLUinImage.setY(fUp);
            m_qptRDinImage.setX(fRight);
            m_qptRDinImage.setY(fDown);
        }
        paintImage();
    }
}

void UI_DispPad::on_SignalZoom(int iZoom)
{
    switch (iZoom)
    {
    case 0:
        m_uiZoomCtrl = 0;
        on_LabelRightDoubleClicked(QPoint(0,0));
        break;
    case 1:
        m_uiZoomCtrl = 1;
        break;
    case 2:
        m_uiZoomCtrl = 2;
        break;
    default:
        m_uiZoomCtrl = 0;
        break;
    }
}








