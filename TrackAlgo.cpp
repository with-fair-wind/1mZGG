#include "TrackAlgo.h"

TrackAlgo::TrackAlgo()
{
#ifndef __linux__
	SYSTEM_INFO sysInfo;
	GetSystemInfo(&sysInfo);
	m_iNumCPUCores = sysInfo.dwNumberOfProcessors;
#else
    m_iNumCPUCores = get_nprocs_conf();
#endif	

    m_iNumCPUCores = 32;

	InitFIFO_Target();
	InitFIFO_Star();
	InitTargetInfo();
	m_iMode = TRACK_INIT;
    m_iTargetID = 0;

    m_pGParam = GlobalParameter::GetInstance();
}
#include <QDataStream>
#include <QDebug>
TrackAlgo::~TrackAlgo()
{
    qDebug() << "TrackAlgo";
}

void TrackAlgo::BubbleSortReverse(vector<int>& vectInput)
{
	int i, j;
	for (i = 0; i < vectInput.size() - 1; i++)          //外层循环控制趟数，总趟数为len-1
		for (j = 0; j < vectInput.size() - 1 - i; j++)  //内层循环为当前i趟数 所需要比较的次数
			if (vectInput[j] < vectInput[j + 1])
				swap(vectInput[j], vectInput[j + 1]);
}

void TrackAlgo::FindMost(vector<pair<int, int>> vectInput, pair<int, int>& pairMost, int& iNumMost)
{
	double dStart = omp_get_wtime();

	set<pair<int, int>> setInput;
	for (int i = 0; i < vectInput.size(); i++)
	{
		setInput.insert(vectInput[i]);
	}

	vector<pair<int, int>> vectInputUnique;
	for (set<pair<int, int>>::iterator iter = setInput.begin(); iter != setInput.end(); iter++)
	{
		vectInputUnique.push_back(*iter);
	}

	vector<vector<int>> vectCountsTemp;
	for (int i = 0; i < m_iNumCPUCores; i++)
	{
		vector<int> vect00;
		for (vector<pair<int, int>>::iterator iter = vectInputUnique.begin(); iter != vectInputUnique.end(); iter++)
		{
			vect00.push_back(0);
		}
		vectCountsTemp.push_back(vect00);
	}

#pragma omp parallel for num_threads(m_iNumCPUCores)
	for (int i = 0; i < vectInputUnique.size(); i++)
	{
		for (int j = 0; j < vectInput.size(); j++)
		{
			if (vectInput[j] == vectInputUnique[i])
			{
				vectCountsTemp[omp_get_thread_num()][i]++;
			}
		}
	}

	vector<int> vectCounts;
	for (vector<pair<int, int>>::iterator iter = vectInputUnique.begin(); iter != vectInputUnique.end(); iter++)
	{
		vectCounts.push_back(0);
	}
	for (int i = 0; i < vectInputUnique.size(); i++)
	{
		for (int j = 0; j < m_iNumCPUCores; j++)
		{
			vectCounts[i] += vectCountsTemp[j][i];
		}
	}

	pair<int, int> pairMostTemp;
	int iNumMostTemp = 0;
	for (int i = 0; i < vectCounts.size(); i++)
	{
		if (vectCounts[i] > iNumMostTemp)
		{
			iNumMostTemp = vectCounts[i];
			pairMostTemp = vectInputUnique[i];
		}
	}
	pairMost = pairMostTemp;
	iNumMost = iNumMostTemp;

	double dEnd = omp_get_wtime();
	//	printf("FindMost using time: %.3fs.\n", dEnd - dStart);
}

int TrackAlgo::CalcStarSpd(vector<sMeasuresInFrame> vectMeasuresInputFIFO, float fRatioFOV, float fRadius, float fThresh, sOpticParams opticparams, pair<float, float>& pairfSpd, pair<float, float>& pairfSpd_AE, int& iNumMost)
{
	int iSize = vectMeasuresInputFIFO.size();
	if (iSize >= 2)
	{
		if (vectMeasuresInputFIFO[iSize - 2].vectStarMeasures.size() > 0 && vectMeasuresInputFIFO[iSize - 1].vectStarMeasures.size() > 0)
		{
			vector<pair<int, int>> vectSpd;
			vector<vector<pair<int, int>>> vectSpd_temp;
			for (int i = 0; i < m_iNumCPUCores; i++)
			{
				vector<pair<int, int>> vect00;
				vectSpd_temp.push_back(vect00);
			}

#pragma omp parallel for num_threads(m_iNumCPUCores)
			for (int i = 0; i < vectMeasuresInputFIFO[iSize - 2].vectStarMeasures.size(); i++)
			{
				for (int j = 0; j < vectMeasuresInputFIFO[iSize - 1].vectStarMeasures.size(); j++)
				{
					if (vectMeasuresInputFIFO[iSize - 2].vectStarMeasures[i].pairfPos.first >(1 - fRatioFOV) * opticparams.iImageWidth / 2		// 中心开窗匹配恒星速度,否则全帧数据量太大,运算时间过长
						&& vectMeasuresInputFIFO[iSize - 2].vectStarMeasures[i].pairfPos.first < (1 + fRatioFOV) * opticparams.iImageWidth / 2
						&& vectMeasuresInputFIFO[iSize - 2].vectStarMeasures[i].pairfPos.second >(1 - fRatioFOV) * opticparams.iImageHeight / 2
						&& vectMeasuresInputFIFO[iSize - 2].vectStarMeasures[i].pairfPos.second  < (1 + fRatioFOV) * opticparams.iImageHeight / 2
						&& vectMeasuresInputFIFO[iSize - 1].vectStarMeasures[j].pairfPos.first >(1 - fRatioFOV) * opticparams.iImageWidth / 2
						&& vectMeasuresInputFIFO[iSize - 1].vectStarMeasures[j].pairfPos.first < (1 + fRatioFOV) * opticparams.iImageWidth / 2
						&& vectMeasuresInputFIFO[iSize - 1].vectStarMeasures[j].pairfPos.second >(1 - fRatioFOV) * opticparams.iImageHeight / 2
						&& vectMeasuresInputFIFO[iSize - 1].vectStarMeasures[j].pairfPos.second < (1 + fRatioFOV) * opticparams.iImageHeight / 2)
					{

                        float fDx = vectMeasuresInputFIFO[iSize - 1].vectStarMeasures[j].pairfPos.first - vectMeasuresInputFIFO[iSize - 2].vectStarMeasures[i].pairfPos.first;
                        float fDy = vectMeasuresInputFIFO[iSize - 1].vectStarMeasures[j].pairfPos.second - vectMeasuresInputFIFO[iSize - 2].vectStarMeasures[i].pairfPos.second;
                        fDx = fDx >= 0 ? floor(fDx) : ceil(fDx);
                        fDy = fDy >= 0 ? floor(fDy) : ceil(fDy);
                        pair<int, int> pairiSpd(fDx, fDy);
                        if (abs(pairiSpd.first) < fRadius && abs(pairiSpd.second) < fRadius)
						{
							vectSpd_temp[omp_get_thread_num()].push_back(pairiSpd);
						}
					}
				}
			}
			for (int i = 0; i < m_iNumCPUCores; i++)
			{
				vectSpd.insert(vectSpd.end(), vectSpd_temp[i].begin(), vectSpd_temp[i].end());
			}
			pair<int, int> pairiSpd;
			FindMost(vectSpd, pairiSpd, iNumMost);
			pairfSpd.first = (float)pairiSpd.first;
			pairfSpd.second = (float)pairiSpd.second;
            double dPeriod = 1.0;
            CalcDiffTime(vectMeasuresInputFIFO[iSize - 2].stimeFrame, vectMeasuresInputFIFO[iSize - 1].stimeFrame, dPeriod);
            float fFrameFreq = 1.0 / dPeriod;
            pairfSpd_AE.first = ((vectMeasuresInputFIFO[iSize - 1].pairfFOVCenterAE.first - vectMeasuresInputFIFO[iSize - 2].pairfFOVCenterAE.first) + pairfSpd.first * opticparams.fPixelScale) * fFrameFreq;
            pairfSpd_AE.second = ((vectMeasuresInputFIFO[iSize - 1].pairfFOVCenterAE.second - vectMeasuresInputFIFO[iSize - 2].pairfFOVCenterAE.second) + pairfSpd.second * opticparams.fPixelScale) * fFrameFreq;
		}
		else
		{
			return 2;
		}
	}
	else
	{
		return 1;
	}

	return 0;
}

//int TrackAlgo::TrackProc_GEO(sMeasuresInFrame measures, vector<sTargetInfo>& vectTargetInfo, bool bFullLEO)
//{
//    if (m_iMode != TRACK_GEO)
//    {
//        return 1;
//    }

//    m_bFullLEO = bFullLEO;

//    m_vectFIFO_Target.push_back(measures);
//    float fThreash = (m_pairfStarSpd.first == 0 && m_pairfStarSpd.second == 0) ? m_settings.fThreashStarMode : m_settings.fThreashGazeMode;
//    if (m_ulFrameSeq == 3)
//    {
//        double dPeriod = 1.0;
//        CalcDiffTime(m_vectFIFO_Target[0].stimeFrame, m_vectFIFO_Target[1].stimeFrame, dPeriod);
//        m_fFrameFreq = 1.0 / dPeriod;
//        if (m_vectFIFO_Target[m_vectFIFO_Target.size()-1].vectTargetMeasures.size() < 10000)
//        {
//            GEO_FindTargets(m_vectFIFO_Target, m_vectTargetInfo, m_pairfStarSpd, m_settings.fRadius, fThreash, m_settings.opticparams);
//        }
//    }
//    else if (m_ulFrameSeq > 3)
//    {
//        GEO_TrackTargets(m_vectFIFO_Target, m_vectTargetInfo, m_pairfStarSpd, m_settings.fRadius, fThreash, m_settings.opticparams, m_settings.fThreashLiving, m_settings.iNumFramesLiving);
//        if (m_vectFIFO_Target[m_vectFIFO_Target.size()-1].vectTargetMeasures.size() < 10000)
//        {
//            GEO_ReFindTargets(m_vectFIFO_Target, m_vectTargetInfo, m_pairfStarSpd, m_settings.fRadius, fThreash, m_settings.opticparams);
//        }
//    }
//    m_ulFrameSeq++;
//    if (m_vectFIFO_Target.size() > 5)
//    {
//        vector<sMeasuresInFrame>::iterator k = m_vectFIFO_Target.begin();
//        m_vectFIFO_Target.erase(k);	// 删除第一个元素
//    }

//    vectTargetInfo = m_vectTargetInfo;

//    return 0;
//}

int TrackAlgo::PushManualSource(sMeasuresInFrame measures, vector<sTargetInfo> &vectTargetInfo)
{
    m_vectFIFO_Target.push_back(measures);

    sTargetInfoInFrame infoinframe;
    infoinframe.stimeFrame = m_vectFIFO_Target.back().stimeFrame;
    infoinframe.ulFrameSeq = m_vectFIFO_Target.back().ulFrameSeq;
    infoinframe.blobMeasure = m_vectFIFO_Target.back().vectTargetMeasures.empty() ? sMeasureBlob() : m_vectFIFO_Target.back().vectTargetMeasures.at(0);
    infoinframe.bValid = !m_vectFIFO_Target.back().vectTargetMeasures.empty();

    if(!m_ulFrameSeq)
    {
        sTargetInfo infoAssoc;
        infoAssoc.vectInfoInFrame.push_back(infoinframe);
        infoAssoc.bLiving |= infoinframe.bValid;
        infoAssoc.fValid = infoinframe.bValid ? 1 : 0;
        vectTargetInfo.push_back(infoAssoc);
    }
    else
    {
        vectTargetInfo.back().vectInfoInFrame.push_back(infoinframe);
        vectTargetInfo.back().bLiving |= infoinframe.bValid;
        vectTargetInfo.back().fValid = ((m_ulFrameSeq) * vectTargetInfo.back().fValid + (infoinframe.bValid ? 1 : 0)) / (m_ulFrameSeq + 1);
    }

    m_ulFrameSeq++;
    if (m_vectFIFO_Target.size() > 5)
    {
        vector<sMeasuresInFrame>::iterator k = m_vectFIFO_Target.begin();
        m_vectFIFO_Target.erase(k);	// 删除第一个元素
    }

    m_vectTargetInfo = vectTargetInfo;

    return 0;
}

int TrackAlgo::TrackProc_RaDec(sMeasuresInFrame measures, vector<sTargetInfo> &vectTargetInfo, bool bFullLEO)
{
    if (m_iMode != TRACK_GEO)
    {
        return 1;
    }

    m_bFullLEO = bFullLEO;

    m_vectFIFO_Target.push_back(measures);
    float fThreash = (m_pairfStarSpd.first == 0 && m_pairfStarSpd.second == 0) ? m_settings.fThreashStarMode : m_settings.fThreashGazeMode;
    if (m_ulFrameSeq == 3)
    {
        double dPeriod = 1.0;
        CalcDiffTime(m_vectFIFO_Target[0].stimeFrame, m_vectFIFO_Target[1].stimeFrame, dPeriod);
        m_fFrameFreq = 1.0 / dPeriod;
        if (m_vectFIFO_Target[m_vectFIFO_Target.size()-1].vectTargetMeasures.size() < 20000)
        {
            RaDec_FindTargets(m_vectFIFO_Target, vectTargetInfo);
        }
    }
    else if (m_ulFrameSeq > 3)
    {
        RaDec_TrackTargets(m_vectFIFO_Target, vectTargetInfo, fThreash, m_settings.opticparams);
        if (m_vectFIFO_Target[m_vectFIFO_Target.size()-1].vectTargetMeasures.size() < 20000)
        {
            RaDec_ReFindTargets(m_vectFIFO_Target, vectTargetInfo);
        }
    }
    m_ulFrameSeq++;
    if (m_vectFIFO_Target.size() > 5)
    {
        vector<sMeasuresInFrame>::iterator k = m_vectFIFO_Target.begin();
        m_vectFIFO_Target.erase(k);	// 删除第一个元素
    }

    m_vectTargetInfo = vectTargetInfo;

    return 0;
}

int TrackAlgo::RaDec_Assoc4(vector<sMeasuresInFrame> vectMeasuresInputFIFO, vector<sTargetInfo>& vectTargetInfo)
{
    double dStart = omp_get_wtime();

    int iSize = vectMeasuresInputFIFO.size();

    double dRaRadius = m_pGParam->m_STrackParams.sRaDecTrackParams.dAssoc_Radius.first / 3600.0;
    double dDecRadius = m_pGParam->m_STrackParams.sRaDecTrackParams.dAssoc_Radius.second / 3600.0;
    double dRaThresh =  m_pGParam->m_STrackParams.sRaDecTrackParams.dAssoc_Thresh.first / 3600.0;
    double dDecThresh = m_pGParam->m_STrackParams.sRaDecTrackParams.dAssoc_Thresh.second / 3600.0;
    double dRaSpdThresh = m_pGParam->m_STrackParams.sRaDecTrackParams.dAssoc_SpdThresh.first / 3600.0;
    double dDecSpdThresh = m_pGParam->m_STrackParams.sRaDecTrackParams.dAssoc_SpdThresh.second / 3600.0;

    m_pGParam->m_SImageProcessorData.fThreshErr = dRaThresh;
    m_pGParam->m_SImageProcessorData.fRadiusT = dRaRadius;

    if (iSize >= 4)
    {
        if (vectMeasuresInputFIFO[iSize - 4].vectTargetMeasures.size() > 0 && vectMeasuresInputFIFO[iSize - 3].vectTargetMeasures.size() > 0 && vectMeasuresInputFIFO[iSize - 2].vectTargetMeasures.size() > 0 && vectMeasuresInputFIFO[iSize - 1].vectTargetMeasures.size() > 0)
        {
            vector<vector<sTargetInfo>> vectTargetInfo_temp;
            for (int i = 0; i < m_iNumCPUCores; i++)
            {
                vector<sTargetInfo> vect00;
                vectTargetInfo_temp.push_back(vect00);
            }

#pragma omp parallel for num_threads(m_iNumCPUCores)
            for (int i = 0; i < vectMeasuresInputFIFO[iSize - 4].vectTargetMeasures.size(); i++)
            {
                for (int j = 0; j < vectMeasuresInputFIFO[iSize - 3].vectTargetMeasures.size(); j++)
                {
                    float fDx1_0 = vectMeasuresInputFIFO[iSize - 3].vectTargetMeasures.at(j).dRa - vectMeasuresInputFIFO[iSize - 4].vectTargetMeasures.at(i).dRa;
                    float fDy1_0 = vectMeasuresInputFIFO[iSize - 3].vectTargetMeasures.at(j).dDec - vectMeasuresInputFIFO[iSize - 4].vectTargetMeasures.at(i).dDec;

                    if (abs(fDx1_0) < dRaRadius && abs(fDy1_0) < dDecRadius
                            && !(abs(fDx1_0) < dRaThresh && abs(fDy1_0) < dDecThresh))
                    {
                        for (int k = 0; k < vectMeasuresInputFIFO[iSize - 2].vectTargetMeasures.size(); k++)
                        {
                            float fDx2_1 = vectMeasuresInputFIFO[iSize - 2].vectTargetMeasures.at(k).dRa - vectMeasuresInputFIFO[iSize - 3].vectTargetMeasures.at(j).dRa;
                            float fDy2_1 = vectMeasuresInputFIFO[iSize - 2].vectTargetMeasures.at(k).dDec - vectMeasuresInputFIFO[iSize - 3].vectTargetMeasures.at(j).dDec;

                            if (abs(fDx2_1) < dRaRadius && abs(fDy2_1) < dDecRadius
                                    && !(abs(fDx2_1) < dRaThresh && abs(fDy2_1) < dDecThresh)
                                    && abs(fDx2_1 - fDx1_0) <= dRaSpdThresh && abs(fDy2_1 - fDy1_0) <= dDecSpdThresh
                                    && (fDx2_1 * fDx1_0 > 0) && (fDy2_1 * fDy1_0 > 0))
                            {
                                for (int m = 0; m < vectMeasuresInputFIFO[iSize - 1].vectTargetMeasures.size(); m++)
                                {
                                    float fDx3_2 = vectMeasuresInputFIFO[iSize - 1].vectTargetMeasures.at(m).dRa - vectMeasuresInputFIFO[iSize - 2].vectTargetMeasures.at(k).dRa;
                                    float fDy3_2 = vectMeasuresInputFIFO[iSize - 1].vectTargetMeasures.at(m).dDec - vectMeasuresInputFIFO[iSize - 2].vectTargetMeasures.at(k).dDec;

                                    if (abs(fDx3_2) < dRaRadius && abs(fDy3_2) < dDecRadius
                                            && !(abs(fDx3_2) < dRaThresh && abs(fDy3_2) < dDecThresh)
                                            && abs(fDx3_2 - fDx2_1) <= dRaSpdThresh && abs(fDy3_2 - fDy2_1) <= dDecSpdThresh
                                            && abs(fDx3_2 - fDx1_0) <= dRaSpdThresh && abs(fDy3_2 - fDy1_0) <= dDecSpdThresh
                                            && (fDx3_2 * fDx2_1 > 0) && (fDy3_2 * fDy2_1 > 0))
                                    {
                                        sTargetInfoInFrame infoinframe0, infoinframe1, infoinframe2, infoinframe3;

                                        infoinframe0.stimeFrame = vectMeasuresInputFIFO[iSize - 4].stimeFrame;
                                        infoinframe0.ulFrameSeq = vectMeasuresInputFIFO[iSize - 4].ulFrameSeq;
                                        infoinframe0.blobMeasure = vectMeasuresInputFIFO[iSize - 4].vectTargetMeasures.at(i);
                                        infoinframe0.bValid = true;

                                        infoinframe1.stimeFrame = vectMeasuresInputFIFO[iSize - 3].stimeFrame;
                                        infoinframe1.ulFrameSeq = vectMeasuresInputFIFO[iSize - 3].ulFrameSeq;
                                        infoinframe1.blobMeasure = vectMeasuresInputFIFO[iSize - 3].vectTargetMeasures.at(j);
                                        infoinframe1.bValid = true;

                                        infoinframe2.stimeFrame = vectMeasuresInputFIFO[iSize - 2].stimeFrame;
                                        infoinframe2.ulFrameSeq = vectMeasuresInputFIFO[iSize - 2].ulFrameSeq;
                                        infoinframe2.blobMeasure = vectMeasuresInputFIFO[iSize - 2].vectTargetMeasures.at(k);
                                        infoinframe2.bValid = true;

                                        infoinframe3.stimeFrame = vectMeasuresInputFIFO[iSize - 1].stimeFrame;
                                        infoinframe3.ulFrameSeq = vectMeasuresInputFIFO[iSize - 1].ulFrameSeq;
                                        infoinframe3.blobMeasure = vectMeasuresInputFIFO[iSize - 1].vectTargetMeasures.at(m);
                                        infoinframe3.bValid = true;

                                        {
                                            sTargetInfo infoAssoc;

                                            infoAssoc.vectInfoInFrame.push_back(infoinframe0);
                                            infoAssoc.vectInfoInFrame.push_back(infoinframe1);
                                            infoAssoc.vectInfoInFrame.push_back(infoinframe2);
                                            infoAssoc.vectInfoInFrame.push_back(infoinframe3);

                                            infoAssoc.pairPredSpdRaDec.first = MedianFloat3(fDx1_0, fDx2_1, fDx3_2);
                                            infoAssoc.pairPredSpdRaDec.second = MedianFloat3(fDy1_0, fDy2_1, fDy3_2);

                                            infoAssoc.pairPredPosRaDec.first = infoinframe3.blobMeasure.dRa + infoAssoc.pairPredSpdRaDec.first;
                                            infoAssoc.pairPredPosRaDec.second = infoinframe3.blobMeasure.dDec + infoAssoc.pairPredSpdRaDec.second;

                                            infoAssoc.fValid = 1.0;
                                            infoAssoc.bLiving = true;

                                            vectTargetInfo_temp[omp_get_thread_num()].push_back(infoAssoc);
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
            for (int i = 0; i < m_iNumCPUCores; i++)
            {
                vectTargetInfo.insert(vectTargetInfo.end(), vectTargetInfo_temp[i].begin(), vectTargetInfo_temp[i].end());
            }

            /// 消除相似轨迹(所有在fThresh范围内的点及后续航迹均被压缩为1条)
            vector<int> vectSeqRemove;
            vector<vector<int>> vectSeqRemove_temp;
            for (int i = 0; i < m_iNumCPUCores; i++)
            {
                vector<int> vect00;
                vectSeqRemove_temp.push_back(vect00);
            }

#pragma omp parallel for num_threads(m_iNumCPUCores)
            for (int i = 0; i < vectTargetInfo.size(); i++)
            {
                for (int j = i + 1; j < vectTargetInfo.size(); j++)	// j = i + 1保证了单向搜索
                {
                    int iSame0 = ((vectTargetInfo.at(i).vectInfoInFrame[0].blobMeasure.dRa == vectTargetInfo.at(j).vectInfoInFrame[0].blobMeasure.dRa)
                            && (vectTargetInfo.at(i).vectInfoInFrame[0].blobMeasure.dDec == vectTargetInfo.at(j).vectInfoInFrame[0].blobMeasure.dDec)) ? 1 : 0;
                    int iSame1 = ((vectTargetInfo.at(i).vectInfoInFrame[1].blobMeasure.dRa == vectTargetInfo.at(j).vectInfoInFrame[1].blobMeasure.dRa)
                            && (vectTargetInfo.at(i).vectInfoInFrame[1].blobMeasure.dDec == vectTargetInfo.at(j).vectInfoInFrame[1].blobMeasure.dDec)) ? 1 : 0;
                    int iSame2 = ((vectTargetInfo.at(i).vectInfoInFrame[2].blobMeasure.dRa == vectTargetInfo.at(j).vectInfoInFrame[2].blobMeasure.dRa)
                            && (vectTargetInfo.at(i).vectInfoInFrame[2].blobMeasure.dDec == vectTargetInfo.at(j).vectInfoInFrame[2].blobMeasure.dDec)) ? 1 : 0;
                    int iSame3 = ((vectTargetInfo.at(i).vectInfoInFrame[3].blobMeasure.dRa == vectTargetInfo.at(j).vectInfoInFrame[3].blobMeasure.dRa)
                            && (vectTargetInfo.at(i).vectInfoInFrame[3].blobMeasure.dDec == vectTargetInfo.at(j).vectInfoInFrame[3].blobMeasure.dDec)) ? 1 : 0;
                    if (iSame0 + iSame1 + iSame2 + iSame3 >= 1)
                    {
                        float f_i_Dx1_0 = vectTargetInfo.at(i).vectInfoInFrame[1].blobMeasure.dRa - vectTargetInfo.at(i).vectInfoInFrame[0].blobMeasure.dRa;
                        float f_i_Dx2_1 = vectTargetInfo.at(i).vectInfoInFrame[2].blobMeasure.dRa - vectTargetInfo.at(i).vectInfoInFrame[1].blobMeasure.dRa;
                        float f_i_Dx3_2 = vectTargetInfo.at(i).vectInfoInFrame[3].blobMeasure.dRa - vectTargetInfo.at(i).vectInfoInFrame[2].blobMeasure.dRa;
                        float f_i_Dy1_0 = vectTargetInfo.at(i).vectInfoInFrame[1].blobMeasure.dDec - vectTargetInfo.at(i).vectInfoInFrame[0].blobMeasure.dDec;
                        float f_i_Dy2_1 = vectTargetInfo.at(i).vectInfoInFrame[2].blobMeasure.dDec - vectTargetInfo.at(i).vectInfoInFrame[1].blobMeasure.dDec;
                        float f_i_Dy3_2 = vectTargetInfo.at(i).vectInfoInFrame[3].blobMeasure.dDec - vectTargetInfo.at(i).vectInfoInFrame[2].blobMeasure.dDec;
                        float f_i_Dx_Mean = (f_i_Dx1_0 + f_i_Dx2_1 + f_i_Dx3_2) / 3.0;
                        float f_i_Dx_Std = pow((pow(f_i_Dx1_0 - f_i_Dx_Mean, 2.0) + pow(f_i_Dx2_1 - f_i_Dx_Mean, 2.0) + pow(f_i_Dx3_2 - f_i_Dx_Mean, 2.0)) / 3.0, 0.5);
                        float f_i_Dy_Mean = (f_i_Dy1_0 + f_i_Dy2_1 + f_i_Dy3_2) / 3.0;
                        float f_i_Dy_Std = pow((pow(f_i_Dy1_0 - f_i_Dy_Mean, 2.0) + pow(f_i_Dy2_1 - f_i_Dy_Mean, 2.0) + pow(f_i_Dy3_2 - f_i_Dy_Mean, 2.0)) / 3.0, 0.5);
                        float f_i_Std = pow(pow(f_i_Dx_Std, 2.0) + pow(f_i_Dy_Std, 2.0), 0.5);


                        float f_j_Dx1_0 = vectTargetInfo.at(j).vectInfoInFrame[1].blobMeasure.dRa - vectTargetInfo.at(j).vectInfoInFrame[0].blobMeasure.dRa;
                        float f_j_Dx2_1 = vectTargetInfo.at(j).vectInfoInFrame[2].blobMeasure.dRa - vectTargetInfo.at(j).vectInfoInFrame[1].blobMeasure.dRa;
                        float f_j_Dx3_2 = vectTargetInfo.at(j).vectInfoInFrame[3].blobMeasure.dRa - vectTargetInfo.at(j).vectInfoInFrame[2].blobMeasure.dRa;
                        float f_j_Dy1_0 = vectTargetInfo.at(j).vectInfoInFrame[1].blobMeasure.dDec - vectTargetInfo.at(j).vectInfoInFrame[0].blobMeasure.dDec;
                        float f_j_Dy2_1 = vectTargetInfo.at(j).vectInfoInFrame[2].blobMeasure.dDec - vectTargetInfo.at(j).vectInfoInFrame[1].blobMeasure.dDec;
                        float f_j_Dy3_2 = vectTargetInfo.at(j).vectInfoInFrame[3].blobMeasure.dDec - vectTargetInfo.at(j).vectInfoInFrame[2].blobMeasure.dDec;
                        float f_j_Dx_Mean = (f_j_Dx1_0 + f_j_Dx2_1 + f_j_Dx3_2) / 3.0;
                        float f_j_Dx_Std = pow((pow(f_j_Dx1_0 - f_j_Dx_Mean, 2.0) + pow(f_j_Dx2_1 - f_j_Dx_Mean, 2.0) + pow(f_j_Dx3_2 - f_j_Dx_Mean, 2.0)) / 3.0, 0.5);
                        float f_j_Dy_Mean = (f_j_Dy1_0 + f_j_Dy2_1 + f_j_Dy3_2) / 3.0;
                        float f_j_Dy_Std = pow((pow(f_j_Dy1_0 - f_j_Dy_Mean, 2.0) + pow(f_j_Dy2_1 - f_j_Dy_Mean, 2.0) + pow(f_j_Dy3_2 - f_j_Dy_Mean, 2.0)) / 3.0, 0.5);
                        float f_j_Std = pow(pow(f_j_Dx_Std, 2.0) + pow(f_j_Dy_Std, 2.0), 0.5);

                        int iTargetRemove = f_i_Std > f_j_Std ? i : j;

                        vectSeqRemove_temp[omp_get_thread_num()].push_back(iTargetRemove);
                    }
                }
            }
            for (int i = 0; i < m_iNumCPUCores; i++)
            {
                vectSeqRemove.insert(vectSeqRemove.end(), vectSeqRemove_temp[i].begin(), vectSeqRemove_temp[i].end());
            }
            if (vectSeqRemove.size() > 0)
            {
                BubbleSortReverse(vectSeqRemove);
                if (vectSeqRemove.size() > 0)
                {
                    vectSeqRemove.erase(unique(vectSeqRemove.begin(), vectSeqRemove.end()), vectSeqRemove.end());
                }
                for (int i = 0; i < vectSeqRemove.size(); i++)
                {
                    if (vectTargetInfo.size() > 0)
                    {
                        vectTargetInfo.erase(vectTargetInfo.begin() + vectSeqRemove[i]);
                    }
                }
            }
        }
        else
        {
            return 2;
        }
    }
    else
    {
        return 1;
    }

    double dEnd = omp_get_wtime();
    //	printf("Assoc3 using time: %.3fs.\n", dEnd - dStart);

    return 0;
}

int TrackAlgo::RaDec_FindTargets(vector<sMeasuresInFrame> vectMeasuresInputFIFO, vector<sTargetInfo>& vectTargetInfo)
{
    int iFIFOSize = vectMeasuresInputFIFO.size();
    if (iFIFOSize >= 4)
    {
        /// 三帧关联
        vector<sTargetInfo> vectTargetInfoTemp;
        if (RaDec_Assoc4(vectMeasuresInputFIFO, vectTargetInfoTemp) == 0)
        {
            for (int i = 0; i < vectTargetInfoTemp.size(); i++)
            {
                if (abs(vectTargetInfoTemp[i].pairPredSpdRaDec.first) > m_pGParam->m_STrackParams.sRaDecTrackParams.dAssoc_Radius.first / 3600.0
                      || abs(vectTargetInfoTemp[i].pairPredSpdRaDec.second) > m_pGParam->m_STrackParams.sRaDecTrackParams.dAssoc_Radius.second / 3600.0)
                {
                    vectTargetInfoTemp[i].bLiving = false;
                }
            }
            vectTargetInfo = vectTargetInfoTemp;
        }
        else
        {
            return 2;
        }
    }
    else
    {
        return 1;
    }

    return 0;
}

int TrackAlgo::RaDec_ReFindTargets(vector<sMeasuresInFrame> vectMeasuresInputFIFO, vector<sTargetInfo>& vectTargetInfo)
{
    int iFIFOSize = vectMeasuresInputFIFO.size();
    if (iFIFOSize >= 5)
    {
        /// 4帧关联
        vector<sTargetInfo> vectTargetInfoTemp;
        if (RaDec_Assoc4(vectMeasuresInputFIFO, vectTargetInfoTemp) == 0)
        {
            /// 消除重复轨迹
            vector<int> vectSeqRemove;
            vector<vector<int>> vectSeqRemove_temp;
            for (int i = 0; i < m_iNumCPUCores; i++)
            {
                vector<int> vect00;
                vectSeqRemove_temp.push_back(vect00);
            }

#pragma omp parallel for num_threads(m_iNumCPUCores)
            for (int i = 0; i < vectTargetInfoTemp.size(); i++)
            {
                for (int j = 0; j < vectTargetInfo.size(); j++)
                {
                    if (vectTargetInfo[j].bLiving)
                    {
                        int iSame0 = ((vectTargetInfoTemp.at(i).vectInfoInFrame[0].blobMeasure.dRa == vectTargetInfo.at(j).vectInfoInFrame[vectTargetInfo.at(j).vectInfoInFrame.size() - 4].blobMeasure.dRa)
                                && (vectTargetInfoTemp.at(i).vectInfoInFrame[0].blobMeasure.dDec == vectTargetInfo.at(j).vectInfoInFrame[vectTargetInfo.at(j).vectInfoInFrame.size() - 4].blobMeasure.dDec)) ? 1 : 0;
                        int iSame1 = ((vectTargetInfoTemp.at(i).vectInfoInFrame[1].blobMeasure.dRa == vectTargetInfo.at(j).vectInfoInFrame[vectTargetInfo.at(j).vectInfoInFrame.size() - 3].blobMeasure.dRa)
                                && (vectTargetInfoTemp.at(i).vectInfoInFrame[1].blobMeasure.dDec == vectTargetInfo.at(j).vectInfoInFrame[vectTargetInfo.at(j).vectInfoInFrame.size() - 3].blobMeasure.dDec)) ? 1 : 0;
                        int iSame2 = ((vectTargetInfoTemp.at(i).vectInfoInFrame[2].blobMeasure.dRa == vectTargetInfo.at(j).vectInfoInFrame[vectTargetInfo.at(j).vectInfoInFrame.size() - 2].blobMeasure.dRa)
                                && (vectTargetInfoTemp.at(i).vectInfoInFrame[2].blobMeasure.dDec == vectTargetInfo.at(j).vectInfoInFrame[vectTargetInfo.at(j).vectInfoInFrame.size() - 2].blobMeasure.dDec)) ? 1 : 0;
                        int iSame3 = ((vectTargetInfoTemp.at(i).vectInfoInFrame[3].blobMeasure.dRa == vectTargetInfo.at(j).vectInfoInFrame[vectTargetInfo.at(j).vectInfoInFrame.size() - 1].blobMeasure.dRa)
                                && (vectTargetInfoTemp.at(i).vectInfoInFrame[3].blobMeasure.dDec == vectTargetInfo.at(j).vectInfoInFrame[vectTargetInfo.at(j).vectInfoInFrame.size() - 1].blobMeasure.dDec)) ? 1 : 0;

                        if (iSame0 + iSame1 + iSame2 + iSame3 > 0)
                        {
                            vectSeqRemove_temp[omp_get_thread_num()].push_back(i);
                        }
                    }
                }
            }
            for (int i = 0; i < m_iNumCPUCores; i++)
            {
                vectSeqRemove.insert(vectSeqRemove.end(), vectSeqRemove_temp[i].begin(), vectSeqRemove_temp[i].end());
            }
            if (vectSeqRemove.size() > 0)
            {
                BubbleSortReverse(vectSeqRemove);
                if (vectSeqRemove.size() > 0)
                {
                    vectSeqRemove.erase(unique(vectSeqRemove.begin(), vectSeqRemove.end()), vectSeqRemove.end());
                }
                for (int i = 0; i < vectSeqRemove.size(); i++)
                {
                    if (vectTargetInfoTemp.size() > 0)
                    {
                        vectTargetInfoTemp.erase(vectTargetInfoTemp.begin() + vectSeqRemove[i]);
                    }
                }
            }

            for (int i = 0; i < vectTargetInfoTemp.size(); i++)
            {
                if (abs(vectTargetInfoTemp[i].pairPredSpdRaDec.first) > m_pGParam->m_STrackParams.sRaDecTrackParams.dAssoc_Radius.first / 3600.0
                          || abs(vectTargetInfoTemp[i].pairPredSpdRaDec.second) > m_pGParam->m_STrackParams.sRaDecTrackParams.dAssoc_Radius.second / 3600.0)
                {
                    vectTargetInfoTemp[i].bLiving = false;
                }
                if (vectTargetInfoTemp[i].bLiving)
                {
                    vectTargetInfo.push_back(vectTargetInfoTemp[i]);
                }
            }
        }
        else
        {
            return 2;
        }
    }
    else
    {
        return 1;
    }

    return 0;
}

int TrackAlgo::RaDec_TrackTargets(vector<sMeasuresInFrame> vectMeasuresInputFIFO, vector<sTargetInfo>& vectTargetInfo, float fThresh, sOpticParams opticparams)
{
    int iFIFOSize = vectMeasuresInputFIFO.size();
    vector<sTargetInfoInFrame> vectValid;
    if (iFIFOSize >= 5)
    {       
        {
            for (int i = 0; i < vectTargetInfo.size(); i++)
            {
                if (vectTargetInfo[i].bLiving)
                {
                    vector<sMeasureBlob> blobSelectTemp;
                    vector<float> fDispTempTemp;
                    for (int j = 0; j < m_iNumCPUCores; j++)
                    {
                        sMeasureBlob blob;
                        blobSelectTemp.push_back(blob);
                        float fDistTemp = 1000000;
                        fDispTempTemp.push_back(fDistTemp);
                    }

                    int iNumCheck = vectTargetInfo[i].vectInfoInFrame.size() > 10 ? 10 : vectTargetInfo[i].vectInfoInFrame.size();
                    int iNumUnValid = 0;
                    for (int j = 0; j < iNumCheck; j++)
                    {
                        if(!vectTargetInfo[i].vectInfoInFrame[vectTargetInfo[i].vectInfoInFrame.size()-(j+1)].bValid)
                            iNumUnValid += 1;
//                        else
//                            break;
                    }
//                    int iNumUnValidTemp = iNumUnValid > 5 ? 5 : iNumUnValid;

//                    float fThreshRa = m_dRaThresh + iNumUnValid * pow(10, getExponent(m_dRaThresh));
//                    float fThreshDec = m_dDecThresh + iNumUnValid * pow(10, getExponent(m_dDecThresh));

                    double dRaThreshInit = m_pGParam->m_STrackParams.sRaDecTrackParams.dTrack_Thresh.first / 3600.0;
                    double dDecThreshInit = m_pGParam->m_STrackParams.sRaDecTrackParams.dTrack_Thresh.second / 3600.0;
                    double dRaFactor = m_pGParam->m_STrackParams.sRaDecTrackParams.dTrack_Factor.first / 3600.0;
                    double dDecFactor = m_pGParam->m_STrackParams.sRaDecTrackParams.dTrack_Factor.second / 3600.0;
                    double dRaSpdFactor = m_pGParam->m_STrackParams.sRaDecTrackParams.dTrack_SpdFactor.first / 3600.0;
                    double dDecSpdFactor = m_pGParam->m_STrackParams.sRaDecTrackParams.dTrack_SpdFactor.second / 3600.0;
                    double dRaSpdThresh = m_pGParam->m_STrackParams.sRaDecTrackParams.dAssoc_SpdThresh.first / 3600.0;
                    double dDecSpdThresh = m_pGParam->m_STrackParams.sRaDecTrackParams.dAssoc_SpdThresh.second/ 3600.0;

                    float fThreshRa = dRaThreshInit + iNumUnValid * dRaFactor;
                    float fThreshDec = dDecThreshInit + iNumUnValid * dDecFactor;

#pragma omp parallel for num_threads(m_iNumCPUCores)
                    for (int j = 0; j < vectMeasuresInputFIFO.at(vectMeasuresInputFIFO.size() - 1).vectTargetMeasures.size(); j++)
                    {
                        float fDistX = vectTargetInfo.at(i).pairPredPosRaDec.first - vectMeasuresInputFIFO.back().vectTargetMeasures.at(j).dRa;
                        float fDistY = vectTargetInfo.at(i).pairPredPosRaDec.second - vectMeasuresInputFIFO.back().vectTargetMeasures.at(j).dDec;
                        float fDist = pow(pow(fDistX, 2.0) + pow(fDistY, 2.0), 0.5);
                        float fDx = vectMeasuresInputFIFO.back().vectTargetMeasures.at(j).dRa - vectTargetInfo[i].vectInfoInFrame.back().blobMeasure.dRa;
                        float fDy = vectMeasuresInputFIFO.back().vectTargetMeasures.at(j).dDec - vectTargetInfo[i].vectInfoInFrame.back().blobMeasure.dDec;

//                        iNumUnValidTemp = iNumUnValid > 7 ? 7 : iNumUnValid;
                        float fSpdRaThresh = dRaSpdThresh + iNumUnValid * dRaSpdFactor;
                        float fSpdDecThresh = dDecSpdThresh + iNumUnValid * dDecSpdFactor;
                        if (abs(fDistX) <= fThreshRa && abs(fDistY) <= fThreshDec
                                && fDist <= fDispTempTemp[omp_get_thread_num()]
                                && abs(fDx - vectTargetInfo.at(i).pairPredSpdRaDec.first) < fSpdRaThresh
                                && abs(fDy - vectTargetInfo.at(i).pairPredSpdRaDec.second) < fSpdDecThresh
                                && fDx * vectTargetInfo.at(i).pairPredSpdRaDec.first > 0 && fDy * vectTargetInfo.at(i).pairPredSpdRaDec.second > 0)
                        {
                            blobSelectTemp[omp_get_thread_num()] = vectMeasuresInputFIFO.at(vectMeasuresInputFIFO.size() - 1).vectTargetMeasures.at(j);
                            fDispTempTemp[omp_get_thread_num()] = fDist;
                        }
                    }

                    sMeasureBlob blobSelect;
                    float fDistTemp = 1000000;
                    for (int j = 0; j < m_iNumCPUCores; j++)
                    {
                        if (fDispTempTemp[j] < fDistTemp)
                        {
                            blobSelect = blobSelectTemp[j];
                            fDistTemp = fDispTempTemp[j];
                        }
                    }

                    sTargetInfoInFrame infoTrack;
                    auto iter = vectMeasuresInputFIFO.end() - 1;
                    if (fDistTemp < 1000000)
                    {
                        bool bReUse = false;
                        for (int m = 0; m < vectValid.size(); m++)
                        {
                            if (blobSelect.dRa == vectValid[m].blobMeasure.dRa
                                    || blobSelect.dDec == vectValid[m].blobMeasure.dDec)
                            {
                                bReUse = true;
                                break;
                            }
                        }
                        if (!bReUse)
                        {
                            infoTrack.stimeFrame = vectMeasuresInputFIFO.at(vectMeasuresInputFIFO.size() - 1).stimeFrame;
                            infoTrack.ulFrameSeq = vectMeasuresInputFIFO.at(vectMeasuresInputFIFO.size() - 1).ulFrameSeq;
                            infoTrack.blobMeasure = blobSelect;
                            infoTrack.bValid = true;
                        }
                        else
                        {
                            infoTrack.stimeFrame = vectMeasuresInputFIFO.at(vectMeasuresInputFIFO.size() - 1).stimeFrame;
                            infoTrack.ulFrameSeq = vectMeasuresInputFIFO.at(vectMeasuresInputFIFO.size() - 1).ulFrameSeq;
                            infoTrack.blobMeasure.dRa = vectTargetInfo[i].pairPredPosRaDec.first;
                            infoTrack.blobMeasure.dDec = vectTargetInfo[i].pairPredPosRaDec.second;
                            infoTrack.bValid = false;
                        }
                    }
                    else
                    {
                        infoTrack.stimeFrame = vectMeasuresInputFIFO.at(vectMeasuresInputFIFO.size() - 1).stimeFrame;
                        infoTrack.ulFrameSeq = vectMeasuresInputFIFO.at(vectMeasuresInputFIFO.size() - 1).ulFrameSeq;
                        infoTrack.blobMeasure.dRa = vectTargetInfo[i].pairPredPosRaDec.first;
                        infoTrack.blobMeasure.dDec = vectTargetInfo[i].pairPredPosRaDec.second;
                        infoTrack.bValid = false;
                    }
                    vectTargetInfo[i].vectInfoInFrame.push_back(infoTrack);
                    vectValid.push_back(infoTrack);

                    int iSize = vectTargetInfo[i].vectInfoInFrame.size();
                    if (iSize <= 5)
                    {
                        vectTargetInfo[i].pairPredSpdRaDec.first = MedianFloat3((vectTargetInfo[i].vectInfoInFrame[iSize - 1].blobMeasure.dRa - vectTargetInfo[i].vectInfoInFrame[iSize - 2].blobMeasure.dRa),
                                (vectTargetInfo[i].vectInfoInFrame[iSize - 2].blobMeasure.dRa - vectTargetInfo[i].vectInfoInFrame[iSize - 3].blobMeasure.dRa),
                                (vectTargetInfo[i].vectInfoInFrame[iSize - 3].blobMeasure.dRa - vectTargetInfo[i].vectInfoInFrame[iSize - 4].blobMeasure.dRa));
                        vectTargetInfo[i].pairPredSpdRaDec.second = MedianFloat3((vectTargetInfo[i].vectInfoInFrame[iSize - 1].blobMeasure.dDec - vectTargetInfo[i].vectInfoInFrame[iSize - 2].blobMeasure.dDec),
                                (vectTargetInfo[i].vectInfoInFrame[iSize - 2].blobMeasure.dDec - vectTargetInfo[i].vectInfoInFrame[iSize - 3].blobMeasure.dDec),
                                (vectTargetInfo[i].vectInfoInFrame[iSize - 3].blobMeasure.dDec - vectTargetInfo[i].vectInfoInFrame[iSize - 4].blobMeasure.dDec));
                    }
                    else if (iSize <= 7)
                    {
                        vectTargetInfo[i].pairPredSpdRaDec.first = MedianFloat5((vectTargetInfo[i].vectInfoInFrame[iSize - 1].blobMeasure.dRa - vectTargetInfo[i].vectInfoInFrame[iSize - 2].blobMeasure.dRa),
                                (vectTargetInfo[i].vectInfoInFrame[iSize - 2].blobMeasure.dRa - vectTargetInfo[i].vectInfoInFrame[iSize - 3].blobMeasure.dRa),
                                (vectTargetInfo[i].vectInfoInFrame[iSize - 3].blobMeasure.dRa - vectTargetInfo[i].vectInfoInFrame[iSize - 4].blobMeasure.dRa),
                                (vectTargetInfo[i].vectInfoInFrame[iSize - 4].blobMeasure.dRa - vectTargetInfo[i].vectInfoInFrame[iSize - 5].blobMeasure.dRa),
                                (vectTargetInfo[i].vectInfoInFrame[iSize - 5].blobMeasure.dRa - vectTargetInfo[i].vectInfoInFrame[iSize - 6].blobMeasure.dRa));
                        vectTargetInfo[i].pairPredSpdRaDec.second = MedianFloat5((vectTargetInfo[i].vectInfoInFrame[iSize - 1].blobMeasure.dDec - vectTargetInfo[i].vectInfoInFrame[iSize - 2].blobMeasure.dDec),
                                (vectTargetInfo[i].vectInfoInFrame[iSize - 2].blobMeasure.dDec - vectTargetInfo[i].vectInfoInFrame[iSize - 3].blobMeasure.dDec),
                                (vectTargetInfo[i].vectInfoInFrame[iSize - 3].blobMeasure.dDec - vectTargetInfo[i].vectInfoInFrame[iSize - 4].blobMeasure.dDec),
                                (vectTargetInfo[i].vectInfoInFrame[iSize - 4].blobMeasure.dDec - vectTargetInfo[i].vectInfoInFrame[iSize - 5].blobMeasure.dDec),
                                (vectTargetInfo[i].vectInfoInFrame[iSize - 5].blobMeasure.dDec - vectTargetInfo[i].vectInfoInFrame[iSize - 6].blobMeasure.dDec));
                    }
                    vectTargetInfo[i].pairPredPosRaDec.first = vectTargetInfo[i].vectInfoInFrame[iSize - 1].blobMeasure.dRa + vectTargetInfo[i].pairPredSpdRaDec.first;
                    vectTargetInfo[i].pairPredPosRaDec.second = vectTargetInfo[i].vectInfoInFrame[iSize - 1].blobMeasure.dDec + vectTargetInfo[i].pairPredSpdRaDec.second;

                    vectTargetInfo[i].fValid = ((iSize - 1) * vectTargetInfo[i].fValid + (vectTargetInfo[i].vectInfoInFrame[iSize - 1].bValid ? 1 : 0)) / iSize;

                    vectTargetInfo[i].bLiving = vectTargetInfo[i].bLiving && (vectTargetInfo[i].pairPredPosRaDec.first > 0 && vectTargetInfo[i].pairPredPosRaDec.first < 360.0
                        && vectTargetInfo[i].pairPredPosRaDec.second > -90.0 && vectTargetInfo[i].pairPredPosRaDec.second < 90.0);

                    iNumCheck = vectTargetInfo[i].vectInfoInFrame.size() > 20 ? 20 : vectTargetInfo[i].vectInfoInFrame.size();
                    iNumUnValid = 0;
                    for (int j = 0; j < iNumCheck; j++)
                    {
                        iNumUnValid += (!vectTargetInfo[i].vectInfoInFrame[vectTargetInfo[i].vectInfoInFrame.size()-(j+1)].bValid) ? 1 : 0;
                    }
                    vectTargetInfo[i].bLiving = vectTargetInfo[i].bLiving && !(iNumUnValid == iNumCheck);
                }
            }
        }
    }
    else
    {
        return 1;
    }

    return 0;
}

int TrackAlgo::TrackProc_GEO(sMeasuresInFrame measures, vector<sTargetInfo>& vectTargetInfo, bool bFullLEO, vector<pair<QString, sMeasureBlob>> vecGEOisbValidBlob)
{
    if (m_iMode != TRACK_GEO)
    {
        return 1;
    }

    m_bFullLEO = bFullLEO;

    m_vectFIFO_Target.push_back(measures);
    float fThreash = (m_pairfStarSpd.first == 0 && m_pairfStarSpd.second == 0) ? m_settings.fThreashStarMode : m_settings.fThreashGazeMode;
    if (m_ulFrameSeq == 3)
    {
        double dPeriod = 1.0;
        CalcDiffTime(m_vectFIFO_Target[0].stimeFrame, m_vectFIFO_Target[1].stimeFrame, dPeriod);
        m_fFrameFreq = 1.0 / dPeriod;
        if (m_vectFIFO_Target[m_vectFIFO_Target.size()-1].vectTargetMeasures.size() < 20000)
        {
            GEO_FindTargets(m_vectFIFO_Target, vectTargetInfo, m_pairfStarSpd, m_settings.fRadius, fThreash, m_settings.opticparams);
        }
    }
    else if (m_ulFrameSeq > 3)
    {
        GEO_TrackTargets(m_vectFIFO_Target, vectTargetInfo, m_pairfStarSpd, m_settings.fRadius, fThreash, m_settings.opticparams, m_settings.fThreashLiving, m_settings.iNumFramesLiving, vecGEOisbValidBlob);
        if (m_vectFIFO_Target[m_vectFIFO_Target.size()-1].vectTargetMeasures.size() < 20000)
        {
            GEO_ReFindTargets(m_vectFIFO_Target, vectTargetInfo, m_pairfStarSpd, m_settings.fRadius, fThreash, m_settings.opticparams);
        }
    }
    m_ulFrameSeq++;
    if (m_vectFIFO_Target.size() > 5)
    {
        vector<sMeasuresInFrame>::iterator k = m_vectFIFO_Target.begin();
        m_vectFIFO_Target.erase(k);	// 删除第一个元素
    }

    m_vectTargetInfo = vectTargetInfo;

    return 0;
}

//int TrackAlgo::GEO_Assoc4(vector<sMeasuresInFrame> vectMeasuresInputFIFO, vector<sTargetInfo>& vectTargetInfo, pair<float, float> pairfStarSpd, float fRadius, float fThresh, sOpticParams opticparams)
//{
//	double dStart = omp_get_wtime();

//	int iSize = vectMeasuresInputFIFO.size();
//    if (iSize >= 4)
//	{
//        if (vectMeasuresInputFIFO[iSize - 4].vectTargetMeasures.size() > 0 && vectMeasuresInputFIFO[iSize - 3].vectTargetMeasures.size() > 0 && vectMeasuresInputFIFO[iSize - 2].vectTargetMeasures.size() > 0 && vectMeasuresInputFIFO[iSize - 1].vectTargetMeasures.size() > 0)
//		{
//			vector<vector<sTargetInfo>> vectTargetInfo_temp;
//			for (int i = 0; i < m_iNumCPUCores; i++)
//			{
//				vector<sTargetInfo> vect00;
//				vectTargetInfo_temp.push_back(vect00);
//			}

//            float fDyStar_soft = pairfStarSpd.second < 0 ? pairfStarSpd.second - 0.001 : pairfStarSpd.second + 0.001;

//#pragma omp parallel for num_threads(m_iNumCPUCores)
//            for (int i = 0; i < vectMeasuresInputFIFO[iSize - 4].vectTargetMeasures.size(); i++)
//			{
//                for (int j = 0; j < vectMeasuresInputFIFO[iSize - 3].vectTargetMeasures.size(); j++)
//				{
//                    float fDx1_0 = vectMeasuresInputFIFO[iSize - 3].vectTargetMeasures.at(j).pairfPos.first - vectMeasuresInputFIFO[iSize - 4].vectTargetMeasures.at(i).pairfPos.first;
//                    float fDy1_0 = vectMeasuresInputFIFO[iSize - 3].vectTargetMeasures.at(j).pairfPos.second - vectMeasuresInputFIFO[iSize - 4].vectTargetMeasures.at(i).pairfPos.second;
//                    float fDy1_0_soft = fDy1_0 < 0 ? fDy1_0 - 0.001 : fDy1_0 + 0.001;
//                    if (abs(fDx1_0) < fRadius && abs(fDy1_0) < fRadius
//                            && !(abs(fDx1_0 - pairfStarSpd.first) < 3 && abs(fDy1_0 - pairfStarSpd.second) < 3)
//                            && abs(atan(fDx1_0/fDy1_0_soft) - atan(pairfStarSpd.first/fDyStar_soft)) > 5.0/180.0*3.1415926)
//					{
//                        for (int k = 0; k < vectMeasuresInputFIFO[iSize - 2].vectTargetMeasures.size(); k++)
//						{
//                            float fDx2_1 = vectMeasuresInputFIFO[iSize - 2].vectTargetMeasures.at(k).pairfPos.first - vectMeasuresInputFIFO[iSize - 3].vectTargetMeasures.at(j).pairfPos.first;
//                            float fDy2_1 = vectMeasuresInputFIFO[iSize - 2].vectTargetMeasures.at(k).pairfPos.second - vectMeasuresInputFIFO[iSize - 3].vectTargetMeasures.at(j).pairfPos.second;
//                            float fDy2_1_soft = fDy2_1 < 0 ? fDy2_1 - 0.001 : fDy2_1 + 0.001;
//                            if (abs(fDx2_1) < fRadius && abs(fDy2_1) < fRadius
//                                    && !(abs(fDx2_1 - pairfStarSpd.first) < 3 && abs(fDy2_1 - pairfStarSpd.second) < 3)
////                                    && abs(fDx2_1 - fDx1_0) <= 2 && abs(fDy2_1 - fDy1_0) <= 2
//                                    && abs(fDx2_1 - fDx1_0) <= 2 && abs(fDy2_1 - fDy1_0) <= 2
//                                    && abs(atan(fDx2_1/fDy2_1_soft) - atan(pairfStarSpd.first/fDyStar_soft)) > 5.0/180.0*3.1415926)
//							{
//                                for (int m = 0; m < vectMeasuresInputFIFO[iSize - 1].vectTargetMeasures.size(); m++)
//                                {
//                                    float fDx3_2 = vectMeasuresInputFIFO[iSize - 1].vectTargetMeasures.at(m).pairfPos.first - vectMeasuresInputFIFO[iSize - 2].vectTargetMeasures.at(k).pairfPos.first;
//                                    float fDy3_2 = vectMeasuresInputFIFO[iSize - 1].vectTargetMeasures.at(m).pairfPos.second - vectMeasuresInputFIFO[iSize - 2].vectTargetMeasures.at(k).pairfPos.second;
//                                    float fDy3_2_soft = fDy3_2 < 0 ? fDy3_2 - 0.001 : fDy3_2 + 0.001;
//                                    if (abs(fDx3_2) < fRadius && abs(fDy3_2) < fRadius
//                                            && !(abs(fDx3_2 - pairfStarSpd.first) < 3 && abs(fDy3_2 - pairfStarSpd.second) < 3)
//                                            && abs(fDx3_2 - fDx2_1) <= 2 && abs(fDy3_2 - fDy2_1) <= 2
//                                            && abs(fDx3_2 - fDx1_0) <= 2 && abs(fDy3_2 - fDy1_0) <= 2
//                                            && abs(atan(fDx3_2/fDy3_2_soft) - atan(pairfStarSpd.first/fDyStar_soft)) > 5.0/180.0*3.1415926)
//                                    {
//                                        sTargetInfoInFrame infoinframe0, infoinframe1, infoinframe2, infoinframe3;

//                                        infoinframe0.stimeFrame = vectMeasuresInputFIFO[iSize - 4].stimeFrame;
//                                        infoinframe0.ulFrameSeq = vectMeasuresInputFIFO[iSize - 4].ulFrameSeq;
//                                        infoinframe0.blobMeasure = vectMeasuresInputFIFO[iSize - 4].vectTargetMeasures.at(i);
//                                        infoinframe0.bValid = true;

//                                        infoinframe1.stimeFrame = vectMeasuresInputFIFO[iSize - 3].stimeFrame;
//                                        infoinframe1.ulFrameSeq = vectMeasuresInputFIFO[iSize - 3].ulFrameSeq;
//                                        infoinframe1.blobMeasure = vectMeasuresInputFIFO[iSize - 3].vectTargetMeasures.at(j);
//                                        infoinframe1.bValid = true;

//                                        infoinframe2.stimeFrame = vectMeasuresInputFIFO[iSize - 2].stimeFrame;
//                                        infoinframe2.ulFrameSeq = vectMeasuresInputFIFO[iSize - 2].ulFrameSeq;
//                                        infoinframe2.blobMeasure = vectMeasuresInputFIFO[iSize - 2].vectTargetMeasures.at(k);
//                                        infoinframe2.bValid = true;

//                                        infoinframe3.stimeFrame = vectMeasuresInputFIFO[iSize - 1].stimeFrame;
//                                        infoinframe3.ulFrameSeq = vectMeasuresInputFIFO[iSize - 1].ulFrameSeq;
//                                        infoinframe3.blobMeasure = vectMeasuresInputFIFO[iSize - 1].vectTargetMeasures.at(m);
//                                        infoinframe3.bValid = true;

//                                        sTargetInfo infoAssoc;

//                                        infoAssoc.vectInfoInFrame.push_back(infoinframe0);
//                                        infoAssoc.vectInfoInFrame.push_back(infoinframe1);
//                                        infoAssoc.vectInfoInFrame.push_back(infoinframe2);
//                                        infoAssoc.vectInfoInFrame.push_back(infoinframe3);

//                                        infoAssoc.pairfPredSpdInFrame.first = MedianFloat3(fDx1_0, fDx2_1, fDx3_2);
//                                        infoAssoc.pairfPredSpdInFrame.second = MedianFloat3(fDy1_0, fDy2_1, fDy3_2);

//                                        infoAssoc.pairfPredPosInFrame.first = infoinframe3.blobMeasure.pairfPos.first + infoAssoc.pairfPredSpdInFrame.first;
//                                        infoAssoc.pairfPredPosInFrame.second = infoinframe3.blobMeasure.pairfPos.second + infoAssoc.pairfPredSpdInFrame.second;

//                                        float fPeriod = 1.0 / m_fFrameFreq;
//                                        infoAssoc.pairfPredSpdAE.first = MedianFloat3((infoinframe1.blobMeasure.pairfPosAE.first - infoinframe0.blobMeasure.pairfPosAE.first),
//                                                                          (infoinframe2.blobMeasure.pairfPosAE.first - infoinframe1.blobMeasure.pairfPosAE.first),
//                                                                          (infoinframe3.blobMeasure.pairfPosAE.first - infoinframe2.blobMeasure.pairfPosAE.first)) / fPeriod;
//                                        infoAssoc.pairfPredSpdAE.second = MedianFloat3((infoinframe1.blobMeasure.pairfPosAE.second - infoinframe0.blobMeasure.pairfPosAE.second),
//                                                                           (infoinframe2.blobMeasure.pairfPosAE.second - infoinframe1.blobMeasure.pairfPosAE.second),
//                                                                           (infoinframe3.blobMeasure.pairfPosAE.second - infoinframe2.blobMeasure.pairfPosAE.second)) / fPeriod;

//                                        infoAssoc.pairfPredPosAE.first = infoinframe3.blobMeasure.pairfPosAE.first + infoAssoc.pairfPredSpdAE.first * fPeriod;
//                                        infoAssoc.pairfPredPosAE.second = infoinframe3.blobMeasure.pairfPosAE.second + infoAssoc.pairfPredSpdAE.second * fPeriod;

//                                        infoAssoc.fValid = 1.0;
//                                        infoAssoc.bLiving = true;

//                                        if (!(abs(pairfStarSpd.second) <= 3
//                                                && abs(infoAssoc.pairfPredSpdInFrame.second) <= 3
//                                                && abs(infoAssoc.pairfPredSpdInFrame.first) >= 10))
//                                            vectTargetInfo_temp[omp_get_thread_num()].push_back(infoAssoc);
//                                    }
//                                }
//							}
//						}
//					}
//				}
//			}
//			for (int i = 0; i < m_iNumCPUCores; i++)
//			{
//				vectTargetInfo.insert(vectTargetInfo.end(), vectTargetInfo_temp[i].begin(), vectTargetInfo_temp[i].end());
//			}

//			/// 消除相似轨迹(所有在fThresh范围内的点及后续航迹均被压缩为1条)
//			vector<int> vectSeqRemove;
//			vector<vector<int>> vectSeqRemove_temp;
//			for (int i = 0; i < m_iNumCPUCores; i++)
//			{
//				vector<int> vect00;
//				vectSeqRemove_temp.push_back(vect00);
//			}

//#pragma omp parallel for num_threads(m_iNumCPUCores)
//			for (int i = 0; i < vectTargetInfo.size(); i++)
//			{
//				for (int j = i + 1; j < vectTargetInfo.size(); j++)	// j = i + 1保证了单向搜索
//				{
//                    int iSame0 = ((vectTargetInfo.at(i).vectInfoInFrame[0].blobMeasure.pairfPos.first == vectTargetInfo.at(j).vectInfoInFrame[0].blobMeasure.pairfPos.first)
//                            && (vectTargetInfo.at(i).vectInfoInFrame[0].blobMeasure.pairfPos.second == vectTargetInfo.at(j).vectInfoInFrame[0].blobMeasure.pairfPos.second)) ? 1 : 0;
//                    int iSame1 = ((vectTargetInfo.at(i).vectInfoInFrame[1].blobMeasure.pairfPos.first == vectTargetInfo.at(j).vectInfoInFrame[1].blobMeasure.pairfPos.first)
//                            && (vectTargetInfo.at(i).vectInfoInFrame[1].blobMeasure.pairfPos.second == vectTargetInfo.at(j).vectInfoInFrame[1].blobMeasure.pairfPos.second)) ? 1 : 0;
//                    int iSame2 = ((vectTargetInfo.at(i).vectInfoInFrame[2].blobMeasure.pairfPos.first == vectTargetInfo.at(j).vectInfoInFrame[2].blobMeasure.pairfPos.first)
//                            && (vectTargetInfo.at(i).vectInfoInFrame[2].blobMeasure.pairfPos.second == vectTargetInfo.at(j).vectInfoInFrame[2].blobMeasure.pairfPos.second)) ? 1 : 0;
//                    int iSame3 = ((vectTargetInfo.at(i).vectInfoInFrame[3].blobMeasure.pairfPos.first == vectTargetInfo.at(j).vectInfoInFrame[3].blobMeasure.pairfPos.first)
//                            && (vectTargetInfo.at(i).vectInfoInFrame[3].blobMeasure.pairfPos.second == vectTargetInfo.at(j).vectInfoInFrame[3].blobMeasure.pairfPos.second)) ? 1 : 0;
//					if (iSame0 + iSame1 + iSame2 >= 1)
//					{
//                        float f_i_Dx1_0 = vectTargetInfo.at(i).vectInfoInFrame[1].blobMeasure.pairfPos.first - vectTargetInfo.at(i).vectInfoInFrame[0].blobMeasure.pairfPos.first;
//                        float f_i_Dx2_1 = vectTargetInfo.at(i).vectInfoInFrame[2].blobMeasure.pairfPos.first - vectTargetInfo.at(i).vectInfoInFrame[1].blobMeasure.pairfPos.first;
//                        float f_i_Dx3_2 = vectTargetInfo.at(i).vectInfoInFrame[3].blobMeasure.pairfPos.first - vectTargetInfo.at(i).vectInfoInFrame[2].blobMeasure.pairfPos.first;
//                        float f_i_Dy1_0 = vectTargetInfo.at(i).vectInfoInFrame[1].blobMeasure.pairfPos.second - vectTargetInfo.at(i).vectInfoInFrame[0].blobMeasure.pairfPos.second;
//                        float f_i_Dy2_1 = vectTargetInfo.at(i).vectInfoInFrame[2].blobMeasure.pairfPos.second - vectTargetInfo.at(i).vectInfoInFrame[1].blobMeasure.pairfPos.second;
//                        float f_i_Dy3_2 = vectTargetInfo.at(i).vectInfoInFrame[3].blobMeasure.pairfPos.second - vectTargetInfo.at(i).vectInfoInFrame[2].blobMeasure.pairfPos.second;
//                        float f_i_Dx_Mean = (f_i_Dx1_0 + f_i_Dx2_1 + f_i_Dx3_2) / 3.0;
//                        float f_i_Dx_Std = pow((pow(f_i_Dx1_0 - f_i_Dx_Mean, 2.0) + pow(f_i_Dx2_1 - f_i_Dx_Mean, 2.0) + pow(f_i_Dx3_2 - f_i_Dx_Mean, 2.0)) / 3.0, 0.5);
//                        float f_i_Dy_Mean = (f_i_Dy1_0 + f_i_Dy2_1 + f_i_Dy3_2) / 3.0;
//                        float f_i_Dy_Std = pow((pow(f_i_Dy1_0 - f_i_Dy_Mean, 2.0) + pow(f_i_Dy2_1 - f_i_Dy_Mean, 2.0) + pow(f_i_Dy3_2 - f_i_Dy_Mean, 2.0)) / 3.0, 0.5);
//                        float f_i_Std = pow(pow(f_i_Dx_Std, 2.0) + pow(f_i_Dy_Std, 2.0), 0.5);


//                        float f_j_Dx1_0 = vectTargetInfo.at(j).vectInfoInFrame[1].blobMeasure.pairfPos.first - vectTargetInfo.at(j).vectInfoInFrame[0].blobMeasure.pairfPos.first;
//                        float f_j_Dx2_1 = vectTargetInfo.at(j).vectInfoInFrame[2].blobMeasure.pairfPos.first - vectTargetInfo.at(j).vectInfoInFrame[1].blobMeasure.pairfPos.first;
//                        float f_j_Dx3_2 = vectTargetInfo.at(j).vectInfoInFrame[3].blobMeasure.pairfPos.first - vectTargetInfo.at(j).vectInfoInFrame[2].blobMeasure.pairfPos.first;
//                        float f_j_Dy1_0 = vectTargetInfo.at(j).vectInfoInFrame[1].blobMeasure.pairfPos.second - vectTargetInfo.at(j).vectInfoInFrame[0].blobMeasure.pairfPos.second;
//                        float f_j_Dy2_1 = vectTargetInfo.at(j).vectInfoInFrame[2].blobMeasure.pairfPos.second - vectTargetInfo.at(j).vectInfoInFrame[1].blobMeasure.pairfPos.second;
//                        float f_j_Dy3_2 = vectTargetInfo.at(j).vectInfoInFrame[3].blobMeasure.pairfPos.second - vectTargetInfo.at(j).vectInfoInFrame[2].blobMeasure.pairfPos.second;
//                        float f_j_Dx_Mean = (f_j_Dx1_0 + f_j_Dx2_1 + f_j_Dx3_2) / 3.0;
//                        float f_j_Dx_Std = pow((pow(f_j_Dx1_0 - f_j_Dx_Mean, 2.0) + pow(f_j_Dx2_1 - f_j_Dx_Mean, 2.0) + pow(f_j_Dx3_2 - f_j_Dx_Mean, 2.0)) / 3.0, 0.5);
//                        float f_j_Dy_Mean = (f_j_Dy1_0 + f_j_Dy2_1 + f_j_Dy3_2) / 3.0;
//                        float f_j_Dy_Std = pow((pow(f_j_Dy1_0 - f_j_Dy_Mean, 2.0) + pow(f_j_Dy2_1 - f_j_Dy_Mean, 2.0) + pow(f_j_Dy3_2 - f_j_Dy_Mean, 2.0)) / 3.0, 0.5);
//                        float f_j_Std = pow(pow(f_j_Dx_Std, 2.0) + pow(f_j_Dy_Std, 2.0), 0.5);

//                        int iTargetRemove = f_i_Std > f_j_Std ? i : j;

//						vectSeqRemove_temp[omp_get_thread_num()].push_back(j);
//					}
//				}
//			}
//			for (int i = 0; i < m_iNumCPUCores; i++)
//			{
//				vectSeqRemove.insert(vectSeqRemove.end(), vectSeqRemove_temp[i].begin(), vectSeqRemove_temp[i].end());
//			}
//			if (vectSeqRemove.size() > 0)
//			{
//				BubbleSortReverse(vectSeqRemove);
//				if (vectSeqRemove.size() > 0)
//				{
//					vectSeqRemove.erase(unique(vectSeqRemove.begin(), vectSeqRemove.end()), vectSeqRemove.end());
//				}
//				for (int i = 0; i < vectSeqRemove.size(); i++)
//				{
//					if (vectTargetInfo.size() > 0)
//					{
//						vectTargetInfo.erase(vectTargetInfo.begin() + vectSeqRemove[i]);
//					}
//				}
//			}
//		}
//		else
//		{
//			return 2;
//		}
//	}
//	else
//	{
//		return 1;
//	}

//	double dEnd = omp_get_wtime();
//	//	printf("Assoc3 using time: %.3fs.\n", dEnd - dStart);

//	return 0;
//}

int TrackAlgo::GEO_Assoc4(vector<sMeasuresInFrame> vectMeasuresInputFIFO, vector<sTargetInfo>& vectTargetInfo, pair<float, float> pairfStarSpd, float fRadius, float fThresh, sOpticParams opticparams)
{
    double dStart = omp_get_wtime();

    int iSize = vectMeasuresInputFIFO.size();

    float fThreshNew = m_pGParam->m_SImageProcessorData.bLooseThresh ? 5.0 : 2.5;
    float fRadiusNew = m_pGParam->m_SImageProcessorData.uiNumStarMatch >= 15 ? 30 : 8.0;
    fRadiusNew = vectMeasuresInputFIFO[iSize - 1].vectTargetMeasures.size() > 3000 ? 8.0 : fRadiusNew;
    fRadiusNew = m_pGParam->m_SImageProcessorData.bLooseThresh ? 50 : fRadiusNew;
    m_pGParam->m_SImageProcessorData.fThreshErr = fThreshNew;
    m_pGParam->m_SImageProcessorData.fRadiusT = fRadiusNew;

    if (iSize >= 4)
    {
        if (vectMeasuresInputFIFO[iSize - 4].vectTargetMeasures.size() > 0 && vectMeasuresInputFIFO[iSize - 3].vectTargetMeasures.size() > 0 && vectMeasuresInputFIFO[iSize - 2].vectTargetMeasures.size() > 0 && vectMeasuresInputFIFO[iSize - 1].vectTargetMeasures.size() > 0)
        {
            vector<vector<sTargetInfo>> vectTargetInfo_temp;
            for (int i = 0; i < m_iNumCPUCores; i++)
            {
                vector<sTargetInfo> vect00;
                vectTargetInfo_temp.push_back(vect00);
            }

            float fDyStar_soft = pairfStarSpd.second < 0 ? pairfStarSpd.second - 0.001 : pairfStarSpd.second + 0.001;

#pragma omp parallel for num_threads(m_iNumCPUCores)
            for (int i = 0; i < vectMeasuresInputFIFO[iSize - 4].vectTargetMeasures.size(); i++)
            {
                for (int j = 0; j < vectMeasuresInputFIFO[iSize - 3].vectTargetMeasures.size(); j++)
                {
                    float fDx1_0 = vectMeasuresInputFIFO[iSize - 3].vectTargetMeasures.at(j).pairfPos.first - vectMeasuresInputFIFO[iSize - 4].vectTargetMeasures.at(i).pairfPos.first;
                    float fDy1_0 = vectMeasuresInputFIFO[iSize - 3].vectTargetMeasures.at(j).pairfPos.second - vectMeasuresInputFIFO[iSize - 4].vectTargetMeasures.at(i).pairfPos.second;
                    float fDy1_0_soft = fDy1_0 < 0 ? fDy1_0 - 0.001 : fDy1_0 + 0.001;
                    if (abs(fDx1_0) < fRadiusNew && abs(fDy1_0) < fRadiusNew
                            && !(abs(fDx1_0 - pairfStarSpd.first) < 3 && abs(fDy1_0 - pairfStarSpd.second) < 3)
                            && abs(atan(fDx1_0/fDy1_0_soft) - atan(pairfStarSpd.first/fDyStar_soft)) > 5.0/180.0*3.1415926)
                    {
                        for (int k = 0; k < vectMeasuresInputFIFO[iSize - 2].vectTargetMeasures.size(); k++)
                        {
                            float fDx2_1 = vectMeasuresInputFIFO[iSize - 2].vectTargetMeasures.at(k).pairfPos.first - vectMeasuresInputFIFO[iSize - 3].vectTargetMeasures.at(j).pairfPos.first;
                            float fDy2_1 = vectMeasuresInputFIFO[iSize - 2].vectTargetMeasures.at(k).pairfPos.second - vectMeasuresInputFIFO[iSize - 3].vectTargetMeasures.at(j).pairfPos.second;
                            float fDy2_1_soft = fDy2_1 < 0 ? fDy2_1 - 0.001 : fDy2_1 + 0.001;
                            if (abs(fDx2_1) < fRadiusNew && abs(fDy2_1) < fRadiusNew
                                    && !(abs(fDx2_1 - pairfStarSpd.first) < 3 && abs(fDy2_1 - pairfStarSpd.second) < 3)
//                                    && abs(fDx2_1 - fDx1_0) <= 2 && abs(fDy2_1 - fDy1_0) <= 2
                                    && abs(fDx2_1 - fDx1_0) <= fThreshNew && abs(fDy2_1 - fDy1_0) <= fThreshNew
                                    && abs(atan(fDx2_1/fDy2_1_soft) - atan(pairfStarSpd.first/fDyStar_soft)) > 5.0/180.0*3.1415926)
                            {
                                for (int m = 0; m < vectMeasuresInputFIFO[iSize - 1].vectTargetMeasures.size(); m++)
                                {
                                    float fDx3_2 = vectMeasuresInputFIFO[iSize - 1].vectTargetMeasures.at(m).pairfPos.first - vectMeasuresInputFIFO[iSize - 2].vectTargetMeasures.at(k).pairfPos.first;
                                    float fDy3_2 = vectMeasuresInputFIFO[iSize - 1].vectTargetMeasures.at(m).pairfPos.second - vectMeasuresInputFIFO[iSize - 2].vectTargetMeasures.at(k).pairfPos.second;
                                    float fDy3_2_soft = fDy3_2 < 0 ? fDy3_2 - 0.001 : fDy3_2 + 0.001;
                                    if (abs(fDx3_2) < fRadiusNew && abs(fDy3_2) < fRadiusNew
                                            && !(abs(fDx3_2 - pairfStarSpd.first) < 3 && abs(fDy3_2 - pairfStarSpd.second) < 3)
                                            && abs(fDx3_2 - fDx2_1) <= fThreshNew && abs(fDy3_2 - fDy2_1) <= fThreshNew
                                            && abs(fDx3_2 - fDx1_0) <= fThreshNew && abs(fDy3_2 - fDy1_0) <= fThreshNew
                                            && abs(atan(fDx3_2/fDy3_2_soft) - atan(pairfStarSpd.first/fDyStar_soft)) > 5.0/180.0*3.1415926)
                                    {
                                        sTargetInfoInFrame infoinframe0, infoinframe1, infoinframe2, infoinframe3;

                                        infoinframe0.stimeFrame = vectMeasuresInputFIFO[iSize - 4].stimeFrame;
                                        infoinframe0.ulFrameSeq = vectMeasuresInputFIFO[iSize - 4].ulFrameSeq;
                                        infoinframe0.blobMeasure = vectMeasuresInputFIFO[iSize - 4].vectTargetMeasures.at(i);
                                        infoinframe0.bValid = true;
                                        double dRm_0 = vectMeasuresInputFIFO[iSize - 4].vectTargetMeasures.at(i).dAlpha;
                                        double dDm_0 = vectMeasuresInputFIFO[iSize - 4].vectTargetMeasures.at(i).dSigma;

                                        infoinframe1.stimeFrame = vectMeasuresInputFIFO[iSize - 3].stimeFrame;
                                        infoinframe1.ulFrameSeq = vectMeasuresInputFIFO[iSize - 3].ulFrameSeq;
                                        infoinframe1.blobMeasure = vectMeasuresInputFIFO[iSize - 3].vectTargetMeasures.at(j);
                                        infoinframe1.bValid = true;
                                        double dRm_1 = vectMeasuresInputFIFO[iSize - 3].vectTargetMeasures.at(j).dAlpha;
                                        double dDm_1 = vectMeasuresInputFIFO[iSize - 3].vectTargetMeasures.at(j).dSigma;

                                        infoinframe2.stimeFrame = vectMeasuresInputFIFO[iSize - 2].stimeFrame;
                                        infoinframe2.ulFrameSeq = vectMeasuresInputFIFO[iSize - 2].ulFrameSeq;
                                        infoinframe2.blobMeasure = vectMeasuresInputFIFO[iSize - 2].vectTargetMeasures.at(k);
                                        infoinframe2.bValid = true;
                                        double dRm_2 = vectMeasuresInputFIFO[iSize - 2].vectTargetMeasures.at(k).dAlpha;
                                        double dDm_2 = vectMeasuresInputFIFO[iSize - 2].vectTargetMeasures.at(k).dSigma;

                                        infoinframe3.stimeFrame = vectMeasuresInputFIFO[iSize - 1].stimeFrame;
                                        infoinframe3.ulFrameSeq = vectMeasuresInputFIFO[iSize - 1].ulFrameSeq;
                                        infoinframe3.blobMeasure = vectMeasuresInputFIFO[iSize - 1].vectTargetMeasures.at(m);
                                        infoinframe3.bValid = true;
                                        double dRm_3 = vectMeasuresInputFIFO[iSize - 1].vectTargetMeasures.at(m).dAlpha;
                                        double dDm_3 = vectMeasuresInputFIFO[iSize - 1].vectTargetMeasures.at(m).dSigma;

                                        double dThreshold = 0.0015/(cos(dDm_0)+0.00000001)/180.0*pi;

                                        if(!((abs(dRm_0 - dRm_1) < dThreshold && abs(dDm_0 - dDm_1) < dThreshold)
                                             || (abs(dRm_0 - dRm_2) < dThreshold && abs(dDm_0 - dDm_2) < dThreshold)
                                             || (abs(dRm_0 - dRm_3) < dThreshold && abs(dDm_0 - dDm_3) < dThreshold)
                                             || (abs(dRm_1 - dRm_2) < dThreshold && abs(dDm_1 - dDm_2) < dThreshold)
                                             || (abs(dRm_1 - dRm_3) < dThreshold && abs(dDm_1 - dDm_3) < dThreshold)
                                             || (abs(dRm_2 - dRm_3) < dThreshold && abs(dDm_2 - dDm_3) < dThreshold)))
                                        {
                                            sTargetInfo infoAssoc;

                                            infoAssoc.vectInfoInFrame.push_back(infoinframe0);
                                            infoAssoc.vectInfoInFrame.push_back(infoinframe1);
                                            infoAssoc.vectInfoInFrame.push_back(infoinframe2);
                                            infoAssoc.vectInfoInFrame.push_back(infoinframe3);

                                            infoAssoc.pairfPredSpdInFrame.first = MedianFloat3(fDx1_0, fDx2_1, fDx3_2);
                                            infoAssoc.pairfPredSpdInFrame.second = MedianFloat3(fDy1_0, fDy2_1, fDy3_2);

                                            infoAssoc.pairfPredPosInFrame.first = infoinframe3.blobMeasure.pairfPos.first + infoAssoc.pairfPredSpdInFrame.first;
                                            infoAssoc.pairfPredPosInFrame.second = infoinframe3.blobMeasure.pairfPos.second + infoAssoc.pairfPredSpdInFrame.second;

                                            float fPeriod = 1.0 / m_fFrameFreq;
                                            infoAssoc.pairfPredSpdAE.first = MedianFloat3((infoinframe1.blobMeasure.pairfPosAE.first - infoinframe0.blobMeasure.pairfPosAE.first),
                                                                              (infoinframe2.blobMeasure.pairfPosAE.first - infoinframe1.blobMeasure.pairfPosAE.first),
                                                                              (infoinframe3.blobMeasure.pairfPosAE.first - infoinframe2.blobMeasure.pairfPosAE.first)) / fPeriod;
                                            infoAssoc.pairfPredSpdAE.second = MedianFloat3((infoinframe1.blobMeasure.pairfPosAE.second - infoinframe0.blobMeasure.pairfPosAE.second),
                                                                               (infoinframe2.blobMeasure.pairfPosAE.second - infoinframe1.blobMeasure.pairfPosAE.second),
                                                                               (infoinframe3.blobMeasure.pairfPosAE.second - infoinframe2.blobMeasure.pairfPosAE.second)) / fPeriod;

                                            infoAssoc.pairfPredPosAE.first = infoinframe3.blobMeasure.pairfPosAE.first + infoAssoc.pairfPredSpdAE.first * fPeriod;
                                            infoAssoc.pairfPredPosAE.second = infoinframe3.blobMeasure.pairfPosAE.second + infoAssoc.pairfPredSpdAE.second * fPeriod;

                                            infoAssoc.fValid = 1.0;
                                            infoAssoc.bLiving = true;

                                            if (!(abs(pairfStarSpd.second) <= 3
                                                    && abs(infoAssoc.pairfPredSpdInFrame.second) <= 3
                                                    && abs(infoAssoc.pairfPredSpdInFrame.first) >= 10))
                                                vectTargetInfo_temp[omp_get_thread_num()].push_back(infoAssoc);
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
            for (int i = 0; i < m_iNumCPUCores; i++)
            {
                vectTargetInfo.insert(vectTargetInfo.end(), vectTargetInfo_temp[i].begin(), vectTargetInfo_temp[i].end());
            }

            /// 消除相似轨迹(所有在fThresh范围内的点及后续航迹均被压缩为1条)
            vector<int> vectSeqRemove;
            vector<vector<int>> vectSeqRemove_temp;
            for (int i = 0; i < m_iNumCPUCores; i++)
            {
                vector<int> vect00;
                vectSeqRemove_temp.push_back(vect00);
            }

#pragma omp parallel for num_threads(m_iNumCPUCores)
            for (int i = 0; i < vectTargetInfo.size(); i++)
            {
                for (int j = i + 1; j < vectTargetInfo.size(); j++)	// j = i + 1保证了单向搜索
                {
                    int iSame0 = ((vectTargetInfo.at(i).vectInfoInFrame[0].blobMeasure.pairfPos.first == vectTargetInfo.at(j).vectInfoInFrame[0].blobMeasure.pairfPos.first)
                            && (vectTargetInfo.at(i).vectInfoInFrame[0].blobMeasure.pairfPos.second == vectTargetInfo.at(j).vectInfoInFrame[0].blobMeasure.pairfPos.second)) ? 1 : 0;
                    int iSame1 = ((vectTargetInfo.at(i).vectInfoInFrame[1].blobMeasure.pairfPos.first == vectTargetInfo.at(j).vectInfoInFrame[1].blobMeasure.pairfPos.first)
                            && (vectTargetInfo.at(i).vectInfoInFrame[1].blobMeasure.pairfPos.second == vectTargetInfo.at(j).vectInfoInFrame[1].blobMeasure.pairfPos.second)) ? 1 : 0;
                    int iSame2 = ((vectTargetInfo.at(i).vectInfoInFrame[2].blobMeasure.pairfPos.first == vectTargetInfo.at(j).vectInfoInFrame[2].blobMeasure.pairfPos.first)
                            && (vectTargetInfo.at(i).vectInfoInFrame[2].blobMeasure.pairfPos.second == vectTargetInfo.at(j).vectInfoInFrame[2].blobMeasure.pairfPos.second)) ? 1 : 0;
                    int iSame3 = ((vectTargetInfo.at(i).vectInfoInFrame[3].blobMeasure.pairfPos.first == vectTargetInfo.at(j).vectInfoInFrame[3].blobMeasure.pairfPos.first)
                            && (vectTargetInfo.at(i).vectInfoInFrame[3].blobMeasure.pairfPos.second == vectTargetInfo.at(j).vectInfoInFrame[3].blobMeasure.pairfPos.second)) ? 1 : 0;
                    if (iSame0 + iSame1 + iSame2 >= 1)
                    {
                        float f_i_Dx1_0 = vectTargetInfo.at(i).vectInfoInFrame[1].blobMeasure.pairfPos.first - vectTargetInfo.at(i).vectInfoInFrame[0].blobMeasure.pairfPos.first;
                        float f_i_Dx2_1 = vectTargetInfo.at(i).vectInfoInFrame[2].blobMeasure.pairfPos.first - vectTargetInfo.at(i).vectInfoInFrame[1].blobMeasure.pairfPos.first;
                        float f_i_Dx3_2 = vectTargetInfo.at(i).vectInfoInFrame[3].blobMeasure.pairfPos.first - vectTargetInfo.at(i).vectInfoInFrame[2].blobMeasure.pairfPos.first;
                        float f_i_Dy1_0 = vectTargetInfo.at(i).vectInfoInFrame[1].blobMeasure.pairfPos.second - vectTargetInfo.at(i).vectInfoInFrame[0].blobMeasure.pairfPos.second;
                        float f_i_Dy2_1 = vectTargetInfo.at(i).vectInfoInFrame[2].blobMeasure.pairfPos.second - vectTargetInfo.at(i).vectInfoInFrame[1].blobMeasure.pairfPos.second;
                        float f_i_Dy3_2 = vectTargetInfo.at(i).vectInfoInFrame[3].blobMeasure.pairfPos.second - vectTargetInfo.at(i).vectInfoInFrame[2].blobMeasure.pairfPos.second;
                        float f_i_Dx_Mean = (f_i_Dx1_0 + f_i_Dx2_1 + f_i_Dx3_2) / 3.0;
                        float f_i_Dx_Std = pow((pow(f_i_Dx1_0 - f_i_Dx_Mean, 2.0) + pow(f_i_Dx2_1 - f_i_Dx_Mean, 2.0) + pow(f_i_Dx3_2 - f_i_Dx_Mean, 2.0)) / 3.0, 0.5);
                        float f_i_Dy_Mean = (f_i_Dy1_0 + f_i_Dy2_1 + f_i_Dy3_2) / 3.0;
                        float f_i_Dy_Std = pow((pow(f_i_Dy1_0 - f_i_Dy_Mean, 2.0) + pow(f_i_Dy2_1 - f_i_Dy_Mean, 2.0) + pow(f_i_Dy3_2 - f_i_Dy_Mean, 2.0)) / 3.0, 0.5);
                        float f_i_Std = pow(pow(f_i_Dx_Std, 2.0) + pow(f_i_Dy_Std, 2.0), 0.5);


                        float f_j_Dx1_0 = vectTargetInfo.at(j).vectInfoInFrame[1].blobMeasure.pairfPos.first - vectTargetInfo.at(j).vectInfoInFrame[0].blobMeasure.pairfPos.first;
                        float f_j_Dx2_1 = vectTargetInfo.at(j).vectInfoInFrame[2].blobMeasure.pairfPos.first - vectTargetInfo.at(j).vectInfoInFrame[1].blobMeasure.pairfPos.first;
                        float f_j_Dx3_2 = vectTargetInfo.at(j).vectInfoInFrame[3].blobMeasure.pairfPos.first - vectTargetInfo.at(j).vectInfoInFrame[2].blobMeasure.pairfPos.first;
                        float f_j_Dy1_0 = vectTargetInfo.at(j).vectInfoInFrame[1].blobMeasure.pairfPos.second - vectTargetInfo.at(j).vectInfoInFrame[0].blobMeasure.pairfPos.second;
                        float f_j_Dy2_1 = vectTargetInfo.at(j).vectInfoInFrame[2].blobMeasure.pairfPos.second - vectTargetInfo.at(j).vectInfoInFrame[1].blobMeasure.pairfPos.second;
                        float f_j_Dy3_2 = vectTargetInfo.at(j).vectInfoInFrame[3].blobMeasure.pairfPos.second - vectTargetInfo.at(j).vectInfoInFrame[2].blobMeasure.pairfPos.second;
                        float f_j_Dx_Mean = (f_j_Dx1_0 + f_j_Dx2_1 + f_j_Dx3_2) / 3.0;
                        float f_j_Dx_Std = pow((pow(f_j_Dx1_0 - f_j_Dx_Mean, 2.0) + pow(f_j_Dx2_1 - f_j_Dx_Mean, 2.0) + pow(f_j_Dx3_2 - f_j_Dx_Mean, 2.0)) / 3.0, 0.5);
                        float f_j_Dy_Mean = (f_j_Dy1_0 + f_j_Dy2_1 + f_j_Dy3_2) / 3.0;
                        float f_j_Dy_Std = pow((pow(f_j_Dy1_0 - f_j_Dy_Mean, 2.0) + pow(f_j_Dy2_1 - f_j_Dy_Mean, 2.0) + pow(f_j_Dy3_2 - f_j_Dy_Mean, 2.0)) / 3.0, 0.5);
                        float f_j_Std = pow(pow(f_j_Dx_Std, 2.0) + pow(f_j_Dy_Std, 2.0), 0.5);

                        int iTargetRemove = f_i_Std > f_j_Std ? i : j;

                        vectSeqRemove_temp[omp_get_thread_num()].push_back(j);
                    }
                }
            }
            for (int i = 0; i < m_iNumCPUCores; i++)
            {
                vectSeqRemove.insert(vectSeqRemove.end(), vectSeqRemove_temp[i].begin(), vectSeqRemove_temp[i].end());
            }
            if (vectSeqRemove.size() > 0)
            {
                BubbleSortReverse(vectSeqRemove);
                if (vectSeqRemove.size() > 0)
                {
                    vectSeqRemove.erase(unique(vectSeqRemove.begin(), vectSeqRemove.end()), vectSeqRemove.end());
                }
                for (int i = 0; i < vectSeqRemove.size(); i++)
                {
                    if (vectTargetInfo.size() > 0)
                    {
                        vectTargetInfo.erase(vectTargetInfo.begin() + vectSeqRemove[i]);
                    }
                }
            }
        }
        else
        {
            return 2;
        }
    }
    else
    {
        return 1;
    }

    double dEnd = omp_get_wtime();
    //	printf("Assoc3 using time: %.3fs.\n", dEnd - dStart);

    return 0;
}

int TrackAlgo::GEO_FindTargets(vector<sMeasuresInFrame> vectMeasuresInputFIFO, vector<sTargetInfo>& vectTargetInfo, pair<float, float> pairfStarSpd, float fRadius, float fThresh, sOpticParams opticparams)
{
	int iFIFOSize = vectMeasuresInputFIFO.size();
    if (iFIFOSize >= 4)
	{
		/// 三帧关联
		vector<sTargetInfo> vectTargetInfoTemp;
        if (GEO_Assoc4(vectMeasuresInputFIFO, vectTargetInfoTemp, pairfStarSpd, fRadius, fThresh, opticparams) == 0)
		{
            for (int i = 0; i < vectTargetInfoTemp.size(); i++)
            {
                if (abs(vectTargetInfoTemp[i].pairfPredSpdInFrame.first) > 100
                      || abs(vectTargetInfoTemp[i].pairfPredSpdInFrame.second) > 100)
//                if (abs(vectTargetInfoTemp[i].pairfPredSpdInFrame.first) > 50.0 / 3600.0 * pi / 180.0
//                      || abs(vectTargetInfoTemp[i].pairfPredSpdInFrame.second) > 50.0 / 3600.0 * pi / 180.0)
                {
                    vectTargetInfoTemp[i].bLiving = false;
                }
            }
			vectTargetInfo = vectTargetInfoTemp;
		}
		else
		{
			return 2;
		}
	}
	else
	{
		return 1;
	}

	return 0;
}

int TrackAlgo::GEO_ReFindTargets(vector<sMeasuresInFrame> vectMeasuresInputFIFO, vector<sTargetInfo>& vectTargetInfo, pair<float, float> pairfStarSpd, float fRadius, float fThresh, sOpticParams opticparams)
{
	int iFIFOSize = vectMeasuresInputFIFO.size();
    if (iFIFOSize >= 5)
	{
        /// 4帧关联
		vector<sTargetInfo> vectTargetInfoTemp;
        if (GEO_Assoc4(vectMeasuresInputFIFO, vectTargetInfoTemp, pairfStarSpd, fRadius, fThresh, opticparams) == 0)
		{
			/// 消除重复轨迹 
			vector<int> vectSeqRemove;
			vector<vector<int>> vectSeqRemove_temp;
			for (int i = 0; i < m_iNumCPUCores; i++)
			{
				vector<int> vect00;
				vectSeqRemove_temp.push_back(vect00);
			}

#pragma omp parallel for num_threads(m_iNumCPUCores)
			for (int i = 0; i < vectTargetInfoTemp.size(); i++)
			{
                for (int j = 0; j < vectTargetInfo.size(); j++)
                {
                    if (vectTargetInfo[j].bLiving)
                    {
                        int iSame0 = ((vectTargetInfoTemp.at(i).vectInfoInFrame[0].blobMeasure.pairfPos.first == vectTargetInfo.at(j).vectInfoInFrame[vectTargetInfo.at(j).vectInfoInFrame.size() - 4].blobMeasure.pairfPos.first)
                                && (vectTargetInfoTemp.at(i).vectInfoInFrame[0].blobMeasure.pairfPos.second == vectTargetInfo.at(j).vectInfoInFrame[vectTargetInfo.at(j).vectInfoInFrame.size() - 4].blobMeasure.pairfPos.second)) ? 1 : 0;
                        int iSame1 = ((vectTargetInfoTemp.at(i).vectInfoInFrame[1].blobMeasure.pairfPos.first == vectTargetInfo.at(j).vectInfoInFrame[vectTargetInfo.at(j).vectInfoInFrame.size() - 3].blobMeasure.pairfPos.first)
                                && (vectTargetInfoTemp.at(i).vectInfoInFrame[1].blobMeasure.pairfPos.second == vectTargetInfo.at(j).vectInfoInFrame[vectTargetInfo.at(j).vectInfoInFrame.size() - 3].blobMeasure.pairfPos.second)) ? 1 : 0;
                        int iSame2 = ((vectTargetInfoTemp.at(i).vectInfoInFrame[2].blobMeasure.pairfPos.first == vectTargetInfo.at(j).vectInfoInFrame[vectTargetInfo.at(j).vectInfoInFrame.size() - 2].blobMeasure.pairfPos.first)
                                && (vectTargetInfoTemp.at(i).vectInfoInFrame[2].blobMeasure.pairfPos.second == vectTargetInfo.at(j).vectInfoInFrame[vectTargetInfo.at(j).vectInfoInFrame.size() - 2].blobMeasure.pairfPos.second)) ? 1 : 0;
                        int iSame3 = ((vectTargetInfoTemp.at(i).vectInfoInFrame[3].blobMeasure.pairfPos.first == vectTargetInfo.at(j).vectInfoInFrame[vectTargetInfo.at(j).vectInfoInFrame.size() - 1].blobMeasure.pairfPos.first)
                                && (vectTargetInfoTemp.at(i).vectInfoInFrame[3].blobMeasure.pairfPos.second == vectTargetInfo.at(j).vectInfoInFrame[vectTargetInfo.at(j).vectInfoInFrame.size() - 1].blobMeasure.pairfPos.second)) ? 1 : 0;

                        if (iSame0 + iSame1 + iSame2 + iSame3 > 0)
                        {
                            vectSeqRemove_temp[omp_get_thread_num()].push_back(i);
                        }
                    }
				}
			}
			for (int i = 0; i < m_iNumCPUCores; i++)
			{
				vectSeqRemove.insert(vectSeqRemove.end(), vectSeqRemove_temp[i].begin(), vectSeqRemove_temp[i].end());
			}
			if (vectSeqRemove.size() > 0)
			{
				BubbleSortReverse(vectSeqRemove);
				if (vectSeqRemove.size() > 0)
				{
					vectSeqRemove.erase(unique(vectSeqRemove.begin(), vectSeqRemove.end()), vectSeqRemove.end());
				}
				for (int i = 0; i < vectSeqRemove.size(); i++)
				{
					if (vectTargetInfoTemp.size() > 0)
					{
						vectTargetInfoTemp.erase(vectTargetInfoTemp.begin() + vectSeqRemove[i]);
					}
				}
			}

			for (int i = 0; i < vectTargetInfoTemp.size(); i++)
			{
                if (abs(vectTargetInfoTemp[i].pairfPredSpdInFrame.first) > 100
                          || abs(vectTargetInfoTemp[i].pairfPredSpdInFrame.second) > 100)
                {
                    vectTargetInfoTemp[i].bLiving = false;
                }
                if (vectTargetInfoTemp[i].bLiving)
                {
                    vectTargetInfo.push_back(vectTargetInfoTemp[i]);
                }
			}
		}
		else
		{
			return 2;
		}
	}
	else
	{
		return 1;
	}

	return 0;
}

int TrackAlgo::GEO_TrackTargets(vector<sMeasuresInFrame> vectMeasuresInputFIFO, vector<sTargetInfo>& vectTargetInfo, pair<float, float> pairfStarSpd, float fRadius, float fThresh, sOpticParams opticparams, float fThreashLiving, int iNumFramesLiving, vector<pair<QString, sMeasureBlob>> vecGEOisbValidBlob)
{
	int iFIFOSize = vectMeasuresInputFIFO.size();    
    vector<sTargetInfoInFrame> vectValid;
    if (iFIFOSize >= 5)
	{
        if (m_bFullLEO)
        {
//            for (int i = 0; i < vectTargetInfo.size(); i++)
//            {
//                if (vectTargetInfo[i].bLiving)
//                {
//                    vector<sMeasureBlob> blobSelectTemp;
//                    vector<float> fDispTempTemp;
//                    for (int j = 0; j < m_iNumCPUCores; j++)
//                    {
//                        sMeasureBlob blob;
//                        blobSelectTemp.push_back(blob);
//                        float fDistTemp = 1000000;
//                        fDispTempTemp.push_back(fDistTemp);
//                    }

//        #pragma omp parallel for num_threads(m_iNumCPUCores)
//                    for (int j = 0; j < vectMeasuresInputFIFO.at(vectMeasuresInputFIFO.size() - 1).vectTargetMeasures.size(); j++)
//                    {
//                        float fDistX = vectTargetInfo[i].pairfPredPosAE.first - vectMeasuresInputFIFO.at(vectMeasuresInputFIFO.size() - 1).vectTargetMeasures.at(j).pairfPosAE.first;
//                        float fDistY = vectTargetInfo[i].pairfPredPosAE.second - vectMeasuresInputFIFO.at(vectMeasuresInputFIFO.size() - 1).vectTargetMeasures.at(j).pairfPosAE.second;
//                        float fDist = pow(pow(fDistX, 2.0) + pow(fDistY, 2.0), 0.5);
//                        if (fDistX < 1.0 && fDistY < 1.0
//                                && fDist <= fDispTempTemp[omp_get_thread_num()])
//                        {
//                            blobSelectTemp[omp_get_thread_num()] = vectMeasuresInputFIFO.at(vectMeasuresInputFIFO.size() - 1).vectTargetMeasures.at(j);
//                            fDispTempTemp[omp_get_thread_num()] = fDist;
//                        }
//                    }

//                    sMeasureBlob blobSelect;
//                    float fDistTemp = 1000000;
//                    for (int j = 0; j < m_iNumCPUCores; j++)
//                    {
//                        if (fDispTempTemp[j] < fDistTemp)
//                        {
//                            blobSelect = blobSelectTemp[j];
//                            fDistTemp = fDispTempTemp[j];
//                        }
//                    }

//                    sTargetInfoInFrame infoTrack;
//                    if (fDistTemp < 1000000)
//                    {
//                        infoTrack.stimeFrame = vectMeasuresInputFIFO.at(vectMeasuresInputFIFO.size() - 1).stimeFrame;
//                        infoTrack.ulFrameSeq = vectMeasuresInputFIFO.at(vectMeasuresInputFIFO.size() - 1).ulFrameSeq;
//                        infoTrack.blobMeasure = blobSelect;
//                        infoTrack.bValid = true;
//                    }
//                    else
//                    {
//                        infoTrack.stimeFrame = vectMeasuresInputFIFO.at(vectMeasuresInputFIFO.size() - 1).stimeFrame;
//                        infoTrack.ulFrameSeq = vectMeasuresInputFIFO.at(vectMeasuresInputFIFO.size() - 1).ulFrameSeq;
//                        infoTrack.blobMeasure.pairfPos = vectTargetInfo[i].pairfPredPosInFrame;
//                        infoTrack.blobMeasure.fMaxX = infoTrack.blobMeasure.pairfPos.first + 5;
//                        infoTrack.blobMeasure.fMinX = infoTrack.blobMeasure.pairfPos.first - 5;
//                        infoTrack.blobMeasure.fMaxY = infoTrack.blobMeasure.pairfPos.second + 5;
//                        infoTrack.blobMeasure.fMinY = infoTrack.blobMeasure.pairfPos.second - 5;
//                        infoTrack.blobMeasure.fArea = 0;
//                        infoTrack.blobMeasure.fDN = 0;
//                        infoTrack.blobMeasure.pairfPosAE = vectTargetInfo[i].pairfPredPosAE;
//                        infoTrack.bValid = false;
//                    }
//                    vectTargetInfo[i].vectInfoInFrame.push_back(infoTrack);
//                    int iSize = vectTargetInfo[i].vectInfoInFrame.size();
//                    vectTargetInfo[i].pairfPredSpdInFrame.first = MedianFloat3((vectTargetInfo[i].vectInfoInFrame[iSize - 1].blobMeasure.pairfPos.first - vectTargetInfo[i].vectInfoInFrame[iSize - 2].blobMeasure.pairfPos.first),
//                            (vectTargetInfo[i].vectInfoInFrame[iSize - 2].blobMeasure.pairfPos.first - vectTargetInfo[i].vectInfoInFrame[iSize - 3].blobMeasure.pairfPos.first),
//                            (vectTargetInfo[i].vectInfoInFrame[iSize - 3].blobMeasure.pairfPos.first - vectTargetInfo[i].vectInfoInFrame[iSize - 4].blobMeasure.pairfPos.first));
//                    vectTargetInfo[i].pairfPredSpdInFrame.second = MedianFloat3((vectTargetInfo[i].vectInfoInFrame[iSize - 1].blobMeasure.pairfPos.second - vectTargetInfo[i].vectInfoInFrame[iSize - 2].blobMeasure.pairfPos.second),
//                            (vectTargetInfo[i].vectInfoInFrame[iSize - 2].blobMeasure.pairfPos.second - vectTargetInfo[i].vectInfoInFrame[iSize - 3].blobMeasure.pairfPos.second),
//                            (vectTargetInfo[i].vectInfoInFrame[iSize - 3].blobMeasure.pairfPos.second - vectTargetInfo[i].vectInfoInFrame[iSize - 4].blobMeasure.pairfPos.second));
//                    vectTargetInfo[i].pairfPredPosInFrame.first = vectTargetInfo[i].vectInfoInFrame[iSize - 1].blobMeasure.pairfPos.first + vectTargetInfo[i].pairfPredSpdInFrame.first;
//                    vectTargetInfo[i].pairfPredPosInFrame.second = vectTargetInfo[i].vectInfoInFrame[iSize - 1].blobMeasure.pairfPos.second + vectTargetInfo[i].pairfPredSpdInFrame.second;
//                    float fPeriod = 1.0 / m_fFrameFreq;
//                    vectTargetInfo[i].pairfPredSpdAE.first = MedianFloat3((vectTargetInfo[i].vectInfoInFrame[iSize - 1].blobMeasure.pairfPosAE.first - vectTargetInfo[i].vectInfoInFrame[iSize - 2].blobMeasure.pairfPosAE.first),
//                            (vectTargetInfo[i].vectInfoInFrame[iSize - 2].blobMeasure.pairfPosAE.first - vectTargetInfo[i].vectInfoInFrame[iSize - 3].blobMeasure.pairfPosAE.first),
//                            (vectTargetInfo[i].vectInfoInFrame[iSize - 3].blobMeasure.pairfPosAE.first - vectTargetInfo[i].vectInfoInFrame[iSize - 4].blobMeasure.pairfPosAE.first)) / fPeriod;
//                    vectTargetInfo[i].pairfPredSpdAE.second = MedianFloat3((vectTargetInfo[i].vectInfoInFrame[iSize - 1].blobMeasure.pairfPosAE.second - vectTargetInfo[i].vectInfoInFrame[iSize - 2].blobMeasure.pairfPosAE.second),
//                            (vectTargetInfo[i].vectInfoInFrame[iSize - 2].blobMeasure.pairfPosAE.second - vectTargetInfo[i].vectInfoInFrame[iSize - 3].blobMeasure.pairfPosAE.second),
//                            (vectTargetInfo[i].vectInfoInFrame[iSize - 3].blobMeasure.pairfPosAE.second - vectTargetInfo[i].vectInfoInFrame[iSize - 4].blobMeasure.pairfPosAE.second)) / fPeriod;
//                    vectTargetInfo[i].pairfPredPosAE.first = vectTargetInfo[i].vectInfoInFrame[iSize - 1].blobMeasure.pairfPosAE.first + vectTargetInfo[i].pairfPredSpdAE.first * fPeriod;
//                    vectTargetInfo[i].pairfPredPosAE.second = vectTargetInfo[i].vectInfoInFrame[iSize - 1].blobMeasure.pairfPosAE.second + vectTargetInfo[i].pairfPredSpdAE.second * fPeriod;
//                    vectTargetInfo[i].fValid = ((iSize - 1) * vectTargetInfo[i].fValid + (vectTargetInfo[i].vectInfoInFrame[iSize - 1].bValid ? 1 : 0)) / iSize;
//                    vectTargetInfo[i].bLiving = vectTargetInfo[i].bLiving
//                            && !(!vectTargetInfo[i].vectInfoInFrame[vectTargetInfo[i].vectInfoInFrame.size()-1].bValid
//                            && !vectTargetInfo[i].vectInfoInFrame[vectTargetInfo[i].vectInfoInFrame.size()-2].bValid
//                            && !vectTargetInfo[i].vectInfoInFrame[vectTargetInfo[i].vectInfoInFrame.size()-3].bValid
//                            && !vectTargetInfo[i].vectInfoInFrame[vectTargetInfo[i].vectInfoInFrame.size()-4].bValid
//                            && !vectTargetInfo[i].vectInfoInFrame[vectTargetInfo[i].vectInfoInFrame.size()-5].bValid);
//                }
//            }

            for (int i = 0; i < vectTargetInfo.size(); i++)
            {
                if (vectTargetInfo[i].bLiving)
                {
                    sMeasureBlob PreBlob;
                    for(int j = 0; j < vecGEOisbValidBlob.size(); j++)
                    {
                        if(vectTargetInfo[i].qstrTargetID == vecGEOisbValidBlob[j].first)
                        {
                            PreBlob = vecGEOisbValidBlob[j].second;
                            break;
                        }
                    }

                    vector<sMeasureBlob> blobSelectTemp;
                    vector<float> fDispTempTemp;
                    for (int j = 0; j < m_iNumCPUCores; j++)
                    {
                        sMeasureBlob blob;
                        blobSelectTemp.push_back(blob);
                        float fDistTemp = 1000000;
                        fDispTempTemp.push_back(fDistTemp);
                    }

                    int iNumCheck = vectTargetInfo[i].vectInfoInFrame.size() > 10 ? 10 : vectTargetInfo[i].vectInfoInFrame.size();
                    int iNumUnValid = 0;
                    for (int j = 0; j < iNumCheck; j++)
                    {
                        iNumUnValid += (!vectTargetInfo[i].vectInfoInFrame[vectTargetInfo[i].vectInfoInFrame.size()-(j+1)].bValid) ? 1 : 0;
                    }
                    int iNumUnValidTemp = iNumUnValid > 3 ? 3 : iNumUnValid;
                    fThresh = fThresh + iNumUnValidTemp * 10;
                    fThresh = 200;

    #pragma omp parallel for num_threads(m_iNumCPUCores)
                    for (int j = 0; j < vectMeasuresInputFIFO.at(vectMeasuresInputFIFO.size() - 1).vectTargetMeasures.size(); j++)
                    {
                        float fDistX = vectTargetInfo.at(i).pairfPredPosInFrame.first - vectMeasuresInputFIFO.at(vectMeasuresInputFIFO.size() - 1).vectTargetMeasures.at(j).pairfPos.first;
                        float fDistY = vectTargetInfo.at(i).pairfPredPosInFrame.second - vectMeasuresInputFIFO.at(vectMeasuresInputFIFO.size() - 1).vectTargetMeasures.at(j).pairfPos.second;
                        float fDist = pow(pow(fDistX, 2.0) + pow(fDistY, 2.0), 0.5);
                        float fDx = vectMeasuresInputFIFO.at(vectMeasuresInputFIFO.size() - 1).vectTargetMeasures.at(j).pairfPos.first
                                - vectTargetInfo[i].vectInfoInFrame[vectTargetInfo[i].vectInfoInFrame.size()-1].blobMeasure.pairfPos.first;
                        float fDy = vectMeasuresInputFIFO.at(vectMeasuresInputFIFO.size() - 1).vectTargetMeasures.at(j).pairfPos.second
                                - vectTargetInfo[i].vectInfoInFrame[vectTargetInfo[i].vectInfoInFrame.size()-1].blobMeasure.pairfPos.second;
//                        iNumUnValidTemp = iNumUnValid > 7 ? 7 : iNumUnValid;
//                        float fSpdErrThresh = 3 + iNumUnValid;
                        if (abs(fDistX) <= fThresh && abs(fDistY) <= fThresh
                                && fDist <= fDispTempTemp[omp_get_thread_num()]/*
                                && abs(fDx - vectTargetInfo.at(i).pairfPredSpdInFrame.first) < fSpdErrThresh
                                && abs(fDy - vectTargetInfo.at(i).pairfPredSpdInFrame.second) < fSpdErrThresh*/)
                        {
                            blobSelectTemp[omp_get_thread_num()] = vectMeasuresInputFIFO.at(vectMeasuresInputFIFO.size() - 1).vectTargetMeasures.at(j);
                            fDispTempTemp[omp_get_thread_num()] = fDist;
                        }
                    }

                    sMeasureBlob blobSelect;
                    float fDistTemp = 1000000;
                    for (int j = 0; j < m_iNumCPUCores; j++)
                    {
                        if (fDispTempTemp[j] < fDistTemp)
                        {
                            blobSelect = blobSelectTemp[j];
                            fDistTemp = fDispTempTemp[j];
                        }
                    }

                    sTargetInfoInFrame infoTrack;
                    if (fDistTemp < 1000000)
                    {
                        bool bReUse = false;
                        for (int m = 0; m < vectValid.size(); m++)
                        {
                            if (blobSelect.pairfPos.first == vectValid[m].blobMeasure.pairfPos.first
                                    || blobSelect.pairfPos.second == vectValid[m].blobMeasure.pairfPos.second)
                            {
                                bReUse = true;
                                break;
                            }
                        }
                        if (!bReUse)
                        {
                            infoTrack.stimeFrame = vectMeasuresInputFIFO.at(vectMeasuresInputFIFO.size() - 1).stimeFrame;
                            infoTrack.ulFrameSeq = vectMeasuresInputFIFO.at(vectMeasuresInputFIFO.size() - 1).ulFrameSeq;
                            infoTrack.blobMeasure = blobSelect;
                            infoTrack.bValid = true;
                        }
                        else
                        {
                            infoTrack.stimeFrame = vectMeasuresInputFIFO.at(vectMeasuresInputFIFO.size() - 1).stimeFrame;
                            infoTrack.ulFrameSeq = vectMeasuresInputFIFO.at(vectMeasuresInputFIFO.size() - 1).ulFrameSeq;
//                            infoTrack.blobMeasure.pairfPos = vectTargetInfo[i].pairfPredPosInFrame;
//                            infoTrack.blobMeasure.fMaxX = infoTrack.blobMeasure.pairfPos.first + 5;
//                            infoTrack.blobMeasure.fMinX = infoTrack.blobMeasure.pairfPos.first - 5;
//                            infoTrack.blobMeasure.fMaxY = infoTrack.blobMeasure.pairfPos.second + 5;
//                            infoTrack.blobMeasure.fMinY = infoTrack.blobMeasure.pairfPos.second - 5;
//                            infoTrack.blobMeasure.fArea = 0;
//                            infoTrack.blobMeasure.fDN = 0;
//                            infoTrack.blobMeasure.pairfPosAE = vectTargetInfo[i].pairfPredPosAE;
                            infoTrack.blobMeasure = PreBlob;
                            infoTrack.bValid = false;
                        }
                    }
                    else
                    {
                        infoTrack.stimeFrame = vectMeasuresInputFIFO.at(vectMeasuresInputFIFO.size() - 1).stimeFrame;
                        infoTrack.ulFrameSeq = vectMeasuresInputFIFO.at(vectMeasuresInputFIFO.size() - 1).ulFrameSeq;
//                        infoTrack.blobMeasure.pairfPos = vectTargetInfo[i].pairfPredPosInFrame;
//                        infoTrack.blobMeasure.fMaxX = infoTrack.blobMeasure.pairfPos.first + 5;
//                        infoTrack.blobMeasure.fMinX = infoTrack.blobMeasure.pairfPos.first - 5;
//                        infoTrack.blobMeasure.fMaxY = infoTrack.blobMeasure.pairfPos.second + 5;
//                        infoTrack.blobMeasure.fMinY = infoTrack.blobMeasure.pairfPos.second - 5;
//                        infoTrack.blobMeasure.fArea = 0;
//                        infoTrack.blobMeasure.fDN = 0;
//                        infoTrack.blobMeasure.pairfPosAE = vectTargetInfo[i].pairfPredPosAE;
                        infoTrack.blobMeasure = PreBlob;
                        infoTrack.bValid = false;
                    }
                    vectTargetInfo[i].vectInfoInFrame.push_back(infoTrack);
                    vectValid.push_back(infoTrack);

                    int iSize = vectTargetInfo[i].vectInfoInFrame.size();
                    if (iSize <= 5)
                    {
                        vectTargetInfo[i].pairfPredSpdInFrame.first = MedianFloat3((vectTargetInfo[i].vectInfoInFrame[iSize - 1].blobMeasure.pairfPos.first - vectTargetInfo[i].vectInfoInFrame[iSize - 2].blobMeasure.pairfPos.first),
                                (vectTargetInfo[i].vectInfoInFrame[iSize - 2].blobMeasure.pairfPos.first - vectTargetInfo[i].vectInfoInFrame[iSize - 3].blobMeasure.pairfPos.first),
                                (vectTargetInfo[i].vectInfoInFrame[iSize - 3].blobMeasure.pairfPos.first - vectTargetInfo[i].vectInfoInFrame[iSize - 4].blobMeasure.pairfPos.first));
                        vectTargetInfo[i].pairfPredSpdInFrame.second = MedianFloat3((vectTargetInfo[i].vectInfoInFrame[iSize - 1].blobMeasure.pairfPos.second - vectTargetInfo[i].vectInfoInFrame[iSize - 2].blobMeasure.pairfPos.second),
                                (vectTargetInfo[i].vectInfoInFrame[iSize - 2].blobMeasure.pairfPos.second - vectTargetInfo[i].vectInfoInFrame[iSize - 3].blobMeasure.pairfPos.second),
                                (vectTargetInfo[i].vectInfoInFrame[iSize - 3].blobMeasure.pairfPos.second - vectTargetInfo[i].vectInfoInFrame[iSize - 4].blobMeasure.pairfPos.second));
                    }
                    else
                    {
                        vectTargetInfo[i].pairfPredSpdInFrame.first = MedianFloat5((vectTargetInfo[i].vectInfoInFrame[iSize - 1].blobMeasure.pairfPos.first - vectTargetInfo[i].vectInfoInFrame[iSize - 2].blobMeasure.pairfPos.first),
                                (vectTargetInfo[i].vectInfoInFrame[iSize - 2].blobMeasure.pairfPos.first - vectTargetInfo[i].vectInfoInFrame[iSize - 3].blobMeasure.pairfPos.first),
                                (vectTargetInfo[i].vectInfoInFrame[iSize - 3].blobMeasure.pairfPos.first - vectTargetInfo[i].vectInfoInFrame[iSize - 4].blobMeasure.pairfPos.first),
                                (vectTargetInfo[i].vectInfoInFrame[iSize - 4].blobMeasure.pairfPos.first - vectTargetInfo[i].vectInfoInFrame[iSize - 5].blobMeasure.pairfPos.first),
                                (vectTargetInfo[i].vectInfoInFrame[iSize - 5].blobMeasure.pairfPos.first - vectTargetInfo[i].vectInfoInFrame[iSize - 6].blobMeasure.pairfPos.first));
                        vectTargetInfo[i].pairfPredSpdInFrame.second = MedianFloat5((vectTargetInfo[i].vectInfoInFrame[iSize - 1].blobMeasure.pairfPos.second - vectTargetInfo[i].vectInfoInFrame[iSize - 2].blobMeasure.pairfPos.second),
                                (vectTargetInfo[i].vectInfoInFrame[iSize - 2].blobMeasure.pairfPos.second - vectTargetInfo[i].vectInfoInFrame[iSize - 3].blobMeasure.pairfPos.second),
                                (vectTargetInfo[i].vectInfoInFrame[iSize - 3].blobMeasure.pairfPos.second - vectTargetInfo[i].vectInfoInFrame[iSize - 4].blobMeasure.pairfPos.second),
                                (vectTargetInfo[i].vectInfoInFrame[iSize - 4].blobMeasure.pairfPos.second - vectTargetInfo[i].vectInfoInFrame[iSize - 5].blobMeasure.pairfPos.second),
                                (vectTargetInfo[i].vectInfoInFrame[iSize - 5].blobMeasure.pairfPos.second - vectTargetInfo[i].vectInfoInFrame[iSize - 6].blobMeasure.pairfPos.second));
                    }
                    vectTargetInfo[i].pairfPredPosInFrame.first = vectTargetInfo[i].vectInfoInFrame[iSize - 1].blobMeasure.pairfPos.first + vectTargetInfo[i].pairfPredSpdInFrame.first;
                    vectTargetInfo[i].pairfPredPosInFrame.second = vectTargetInfo[i].vectInfoInFrame[iSize - 1].blobMeasure.pairfPos.second + vectTargetInfo[i].pairfPredSpdInFrame.second;
                    float fPeriod = 1.0 / m_fFrameFreq;
                    vectTargetInfo[i].pairfPredSpdAE.first = MedianFloat3((vectTargetInfo[i].vectInfoInFrame[iSize - 1].blobMeasure.pairfPosAE.first - vectTargetInfo[i].vectInfoInFrame[iSize - 2].blobMeasure.pairfPosAE.first),
                            (vectTargetInfo[i].vectInfoInFrame[iSize - 2].blobMeasure.pairfPosAE.first - vectTargetInfo[i].vectInfoInFrame[iSize - 3].blobMeasure.pairfPosAE.first),
                            (vectTargetInfo[i].vectInfoInFrame[iSize - 3].blobMeasure.pairfPosAE.first - vectTargetInfo[i].vectInfoInFrame[iSize - 4].blobMeasure.pairfPosAE.first)) / fPeriod;
                    vectTargetInfo[i].pairfPredSpdAE.second = MedianFloat3((vectTargetInfo[i].vectInfoInFrame[iSize - 1].blobMeasure.pairfPosAE.second - vectTargetInfo[i].vectInfoInFrame[iSize - 2].blobMeasure.pairfPosAE.second),
                            (vectTargetInfo[i].vectInfoInFrame[iSize - 2].blobMeasure.pairfPosAE.second - vectTargetInfo[i].vectInfoInFrame[iSize - 3].blobMeasure.pairfPosAE.second),
                            (vectTargetInfo[i].vectInfoInFrame[iSize - 3].blobMeasure.pairfPosAE.second - vectTargetInfo[i].vectInfoInFrame[iSize - 4].blobMeasure.pairfPosAE.second)) / fPeriod;
                    vectTargetInfo[i].pairfPredPosAE.first = vectTargetInfo[i].vectInfoInFrame[iSize - 1].blobMeasure.pairfPosAE.first + vectTargetInfo[i].pairfPredSpdAE.first * fPeriod;
                    vectTargetInfo[i].pairfPredPosAE.second = vectTargetInfo[i].vectInfoInFrame[iSize - 1].blobMeasure.pairfPosAE.second + vectTargetInfo[i].pairfPredSpdAE.second * fPeriod;
                    vectTargetInfo[i].fValid = ((iSize - 1) * vectTargetInfo[i].fValid + (vectTargetInfo[i].vectInfoInFrame[iSize - 1].bValid ? 1 : 0)) / iSize;

    //                if (vectTargetInfo[i].vectInfoInFrame.size() <= 6)
    //                {
    //                    float fDX = vectTargetInfo[i].vectInfoInFrame[vectTargetInfo[i].vectInfoInFrame.size()-1].blobMeasure.pairfPos.first - vectTargetInfo[i].vectInfoInFrame[vectTargetInfo[i].vectInfoInFrame.size()-2].blobMeasure.pairfPos.first;
    //                    float fDY = vectTargetInfo[i].vectInfoInFrame[vectTargetInfo[i].vectInfoInFrame.size()-1].blobMeasure.pairfPos.second - vectTargetInfo[i].vectInfoInFrame[vectTargetInfo[i].vectInfoInFrame.size()-2].blobMeasure.pairfPos.second;
    //                    float fDX1 = vectTargetInfo[i].vectInfoInFrame[vectTargetInfo[i].vectInfoInFrame.size()-2].blobMeasure.pairfPos.first - vectTargetInfo[i].vectInfoInFrame[vectTargetInfo[i].vectInfoInFrame.size()-3].blobMeasure.pairfPos.first;
    //                    float fDY1 = vectTargetInfo[i].vectInfoInFrame[vectTargetInfo[i].vectInfoInFrame.size()-2].blobMeasure.pairfPos.second - vectTargetInfo[i].vectInfoInFrame[vectTargetInfo[i].vectInfoInFrame.size()-3].blobMeasure.pairfPos.second;
    //                    vectTargetInfo[i].bLiving = !(abs(fDX - pairfStarSpd.first) < 3 && abs(fDY - pairfStarSpd.second) < 3)
    //                            && (abs(fDX - fDX1) < 3 && abs(fDY - fDY1) < 3);
    //                }

                    vectTargetInfo[i].bLiving = vectTargetInfo[i].bLiving && (vectTargetInfo[i].pairfPredPosInFrame.first > 0 && vectTargetInfo[i].pairfPredPosInFrame.first < opticparams.iImageWidth
                        && vectTargetInfo[i].pairfPredPosInFrame.second > 0 && vectTargetInfo[i].pairfPredPosInFrame.second < opticparams.iImageHeight);

                    iNumCheck = vectTargetInfo[i].vectInfoInFrame.size() > 20 ? 20 : vectTargetInfo[i].vectInfoInFrame.size();
                    iNumUnValid = 0;
                    for (int j = 0; j < iNumCheck; j++)
                    {
                        iNumUnValid += (!vectTargetInfo[i].vectInfoInFrame[vectTargetInfo[i].vectInfoInFrame.size()-(j+1)].bValid) ? 1 : 0;
                    }
                    vectTargetInfo[i].bLiving = vectTargetInfo[i].bLiving && !(iNumUnValid == iNumCheck);

                    if (vectTargetInfo[i].vectInfoInFrame.size() >= 2)
                    {
                        double dRm_1 = vectTargetInfo[i].vectInfoInFrame[vectTargetInfo[i].vectInfoInFrame.size()-1].blobMeasure.dAlpha;
                        double dRm_0 = vectTargetInfo[i].vectInfoInFrame[vectTargetInfo[i].vectInfoInFrame.size()-2].blobMeasure.dAlpha;
                        double dDm_1 = vectTargetInfo[i].vectInfoInFrame[vectTargetInfo[i].vectInfoInFrame.size()-1].blobMeasure.dSigma;
                        double dDm_0 = vectTargetInfo[i].vectInfoInFrame[vectTargetInfo[i].vectInfoInFrame.size()-2].blobMeasure.dSigma;

                        double dThreshold = 0.0015/(cos(dDm_0)+0.00000001)/180.0*pi;

                        vectTargetInfo[i].bLiving = !(abs(dRm_0 - dRm_1) < dThreshold && abs(dDm_0 - dDm_1) < dThreshold);
                    }

                }
            }
        }
        else
        {
            for (int i = 0; i < vectTargetInfo.size(); i++)
            {
                if (vectTargetInfo[i].bLiving)
                {
                    sMeasureBlob PreBlob;
                    for(int j = 0; j < vecGEOisbValidBlob.size(); j++)
                    {
                        if(vectTargetInfo[i].qstrTargetID == vecGEOisbValidBlob[j].first)
                        {
                            PreBlob = vecGEOisbValidBlob[j].second;
                            break;
                        }
                    }

                    vector<sMeasureBlob> blobSelectTemp;
                    vector<float> fDispTempTemp;
                    for (int j = 0; j < m_iNumCPUCores; j++)
                    {
                        sMeasureBlob blob;
                        blobSelectTemp.push_back(blob);
                        float fDistTemp = 1000000;
                        fDispTempTemp.push_back(fDistTemp);
                    }

                    int iNumCheck = vectTargetInfo[i].vectInfoInFrame.size() > 10 ? 10 : vectTargetInfo[i].vectInfoInFrame.size();
                    int iNumUnValid = 0;
                    for (int j = 0; j < iNumCheck; j++)
                    {
                        iNumUnValid += (!vectTargetInfo[i].vectInfoInFrame[vectTargetInfo[i].vectInfoInFrame.size()-(j+1)].bValid) ? 1 : 0;
                    }
                    int iNumUnValidTemp = iNumUnValid > 5 ? 5 : iNumUnValid;
                    fThresh = fThresh + iNumUnValidTemp * 10;

    #pragma omp parallel for num_threads(m_iNumCPUCores)
                    for (int j = 0; j < vectMeasuresInputFIFO.at(vectMeasuresInputFIFO.size() - 1).vectTargetMeasures.size(); j++)
                    {
                        float fDistX = vectTargetInfo.at(i).pairfPredPosInFrame.first - vectMeasuresInputFIFO.at(vectMeasuresInputFIFO.size() - 1).vectTargetMeasures.at(j).pairfPos.first;
                        float fDistY = vectTargetInfo.at(i).pairfPredPosInFrame.second - vectMeasuresInputFIFO.at(vectMeasuresInputFIFO.size() - 1).vectTargetMeasures.at(j).pairfPos.second;
                        float fDist = pow(pow(fDistX, 2.0) + pow(fDistY, 2.0), 0.5);
                        float fDx = vectMeasuresInputFIFO.at(vectMeasuresInputFIFO.size() - 1).vectTargetMeasures.at(j).pairfPos.first
                                - vectTargetInfo[i].vectInfoInFrame[vectTargetInfo[i].vectInfoInFrame.size()-1].blobMeasure.pairfPos.first;
                        float fDy = vectMeasuresInputFIFO.at(vectMeasuresInputFIFO.size() - 1).vectTargetMeasures.at(j).pairfPos.second
                                - vectTargetInfo[i].vectInfoInFrame[vectTargetInfo[i].vectInfoInFrame.size()-1].blobMeasure.pairfPos.second;
                        iNumUnValidTemp = iNumUnValid > 7 ? 7 : iNumUnValid;
                        float fSpdErrThresh = 5 + iNumUnValid;
                        if (abs(fDistX) <= fThresh && abs(fDistY) <= fThresh
                                && fDist <= fDispTempTemp[omp_get_thread_num()]
                                && abs(fDx - vectTargetInfo.at(i).pairfPredSpdInFrame.first) < fSpdErrThresh
                                && abs(fDy - vectTargetInfo.at(i).pairfPredSpdInFrame.second) < fSpdErrThresh)
                        {
                            blobSelectTemp[omp_get_thread_num()] = vectMeasuresInputFIFO.at(vectMeasuresInputFIFO.size() - 1).vectTargetMeasures.at(j);
                            fDispTempTemp[omp_get_thread_num()] = fDist;
                        }
                    }

                    sMeasureBlob blobSelect;
                    float fDistTemp = 1000000;
                    for (int j = 0; j < m_iNumCPUCores; j++)
                    {
                        if (fDispTempTemp[j] < fDistTemp)
                        {
                            blobSelect = blobSelectTemp[j];
                            fDistTemp = fDispTempTemp[j];
                        }
                    }

                    sTargetInfoInFrame infoTrack;
                    auto iter = vectMeasuresInputFIFO.end() - 1;
                    if (fDistTemp < 1000000)
                    {
                        bool bReUse = false;
                        for (int m = 0; m < vectValid.size(); m++)
                        {
                            if (blobSelect.pairfPos.first == vectValid[m].blobMeasure.pairfPos.first
                                    || blobSelect.pairfPos.second == vectValid[m].blobMeasure.pairfPos.second)
                            {
                                bReUse = true;
                                break;
                            }
                        }
                        if (!bReUse)
                        {                            
                            infoTrack.stimeFrame = vectMeasuresInputFIFO.at(vectMeasuresInputFIFO.size() - 1).stimeFrame;
                            infoTrack.ulFrameSeq = vectMeasuresInputFIFO.at(vectMeasuresInputFIFO.size() - 1).ulFrameSeq;
                            infoTrack.blobMeasure = blobSelect;
                            infoTrack.bValid = true;
                        }
                        else
                        {
                            infoTrack.stimeFrame = vectMeasuresInputFIFO.at(vectMeasuresInputFIFO.size() - 1).stimeFrame;
                            infoTrack.ulFrameSeq = vectMeasuresInputFIFO.at(vectMeasuresInputFIFO.size() - 1).ulFrameSeq;
//                            infoTrack.blobMeasure.pairfPos = vectTargetInfo[i].pairfPredPosInFrame;
//                            infoTrack.blobMeasure.fMaxX = infoTrack.blobMeasure.pairfPos.first + 5;
//                            infoTrack.blobMeasure.fMinX = infoTrack.blobMeasure.pairfPos.first - 5;
//                            infoTrack.blobMeasure.fMaxY = infoTrack.blobMeasure.pairfPos.second + 5;
//                            infoTrack.blobMeasure.fMinY = infoTrack.blobMeasure.pairfPos.second - 5;
//                            infoTrack.blobMeasure.fArea = 0;
//                            infoTrack.blobMeasure.fDN = 0;
//                            infoTrack.blobMeasure.pairfPosAE = vectTargetInfo[i].pairfPredPosAE;
                            infoTrack.blobMeasure = PreBlob;
                            infoTrack.bValid = false;                           
                        }
                    }
                    else
                    {
                        infoTrack.stimeFrame = vectMeasuresInputFIFO.at(vectMeasuresInputFIFO.size() - 1).stimeFrame;
                        infoTrack.ulFrameSeq = vectMeasuresInputFIFO.at(vectMeasuresInputFIFO.size() - 1).ulFrameSeq;
//                        infoTrack.blobMeasure.pairfPos = vectTargetInfo[i].pairfPredPosInFrame;
//                        infoTrack.blobMeasure.fMaxX = infoTrack.blobMeasure.pairfPos.first + 5;
//                        infoTrack.blobMeasure.fMinX = infoTrack.blobMeasure.pairfPos.first - 5;
//                        infoTrack.blobMeasure.fMaxY = infoTrack.blobMeasure.pairfPos.second + 5;
//                        infoTrack.blobMeasure.fMinY = infoTrack.blobMeasure.pairfPos.second - 5;
//                        infoTrack.blobMeasure.fArea = 0;
//                        infoTrack.blobMeasure.fDN = 0;
//                        infoTrack.blobMeasure.pairfPosAE = vectTargetInfo[i].pairfPredPosAE;
                        infoTrack.blobMeasure = PreBlob;
                        infoTrack.bValid = false;                        
                    }
                    vectTargetInfo[i].vectInfoInFrame.push_back(infoTrack);
                    vectValid.push_back(infoTrack);

                    int iSize = vectTargetInfo[i].vectInfoInFrame.size();
                    if (iSize <= 5)
                    {
                        vectTargetInfo[i].pairfPredSpdInFrame.first = MedianFloat3((vectTargetInfo[i].vectInfoInFrame[iSize - 1].blobMeasure.pairfPos.first - vectTargetInfo[i].vectInfoInFrame[iSize - 2].blobMeasure.pairfPos.first),
                                (vectTargetInfo[i].vectInfoInFrame[iSize - 2].blobMeasure.pairfPos.first - vectTargetInfo[i].vectInfoInFrame[iSize - 3].blobMeasure.pairfPos.first),
                                (vectTargetInfo[i].vectInfoInFrame[iSize - 3].blobMeasure.pairfPos.first - vectTargetInfo[i].vectInfoInFrame[iSize - 4].blobMeasure.pairfPos.first));
                        vectTargetInfo[i].pairfPredSpdInFrame.second = MedianFloat3((vectTargetInfo[i].vectInfoInFrame[iSize - 1].blobMeasure.pairfPos.second - vectTargetInfo[i].vectInfoInFrame[iSize - 2].blobMeasure.pairfPos.second),
                                (vectTargetInfo[i].vectInfoInFrame[iSize - 2].blobMeasure.pairfPos.second - vectTargetInfo[i].vectInfoInFrame[iSize - 3].blobMeasure.pairfPos.second),
                                (vectTargetInfo[i].vectInfoInFrame[iSize - 3].blobMeasure.pairfPos.second - vectTargetInfo[i].vectInfoInFrame[iSize - 4].blobMeasure.pairfPos.second));
                    }
                    else if (iSize <= 7)
                    {
                        vectTargetInfo[i].pairfPredSpdInFrame.first = MedianFloat5((vectTargetInfo[i].vectInfoInFrame[iSize - 1].blobMeasure.pairfPos.first - vectTargetInfo[i].vectInfoInFrame[iSize - 2].blobMeasure.pairfPos.first),
                                (vectTargetInfo[i].vectInfoInFrame[iSize - 2].blobMeasure.pairfPos.first - vectTargetInfo[i].vectInfoInFrame[iSize - 3].blobMeasure.pairfPos.first),
                                (vectTargetInfo[i].vectInfoInFrame[iSize - 3].blobMeasure.pairfPos.first - vectTargetInfo[i].vectInfoInFrame[iSize - 4].blobMeasure.pairfPos.first),
                                (vectTargetInfo[i].vectInfoInFrame[iSize - 4].blobMeasure.pairfPos.first - vectTargetInfo[i].vectInfoInFrame[iSize - 5].blobMeasure.pairfPos.first),
                                (vectTargetInfo[i].vectInfoInFrame[iSize - 5].blobMeasure.pairfPos.first - vectTargetInfo[i].vectInfoInFrame[iSize - 6].blobMeasure.pairfPos.first));
                        vectTargetInfo[i].pairfPredSpdInFrame.second = MedianFloat5((vectTargetInfo[i].vectInfoInFrame[iSize - 1].blobMeasure.pairfPos.second - vectTargetInfo[i].vectInfoInFrame[iSize - 2].blobMeasure.pairfPos.second),
                                (vectTargetInfo[i].vectInfoInFrame[iSize - 2].blobMeasure.pairfPos.second - vectTargetInfo[i].vectInfoInFrame[iSize - 3].blobMeasure.pairfPos.second),
                                (vectTargetInfo[i].vectInfoInFrame[iSize - 3].blobMeasure.pairfPos.second - vectTargetInfo[i].vectInfoInFrame[iSize - 4].blobMeasure.pairfPos.second),
                                (vectTargetInfo[i].vectInfoInFrame[iSize - 4].blobMeasure.pairfPos.second - vectTargetInfo[i].vectInfoInFrame[iSize - 5].blobMeasure.pairfPos.second),
                                (vectTargetInfo[i].vectInfoInFrame[iSize - 5].blobMeasure.pairfPos.second - vectTargetInfo[i].vectInfoInFrame[iSize - 6].blobMeasure.pairfPos.second));
                    }
                    vectTargetInfo[i].pairfPredPosInFrame.first = vectTargetInfo[i].vectInfoInFrame[iSize - 1].blobMeasure.pairfPos.first + vectTargetInfo[i].pairfPredSpdInFrame.first;
                    vectTargetInfo[i].pairfPredPosInFrame.second = vectTargetInfo[i].vectInfoInFrame[iSize - 1].blobMeasure.pairfPos.second + vectTargetInfo[i].pairfPredSpdInFrame.second;
                    float fPeriod = 1.0 / m_fFrameFreq;
                    vectTargetInfo[i].pairfPredSpdAE.first = MedianFloat3((vectTargetInfo[i].vectInfoInFrame[iSize - 1].blobMeasure.pairfPosAE.first - vectTargetInfo[i].vectInfoInFrame[iSize - 2].blobMeasure.pairfPosAE.first),
                            (vectTargetInfo[i].vectInfoInFrame[iSize - 2].blobMeasure.pairfPosAE.first - vectTargetInfo[i].vectInfoInFrame[iSize - 3].blobMeasure.pairfPosAE.first),
                            (vectTargetInfo[i].vectInfoInFrame[iSize - 3].blobMeasure.pairfPosAE.first - vectTargetInfo[i].vectInfoInFrame[iSize - 4].blobMeasure.pairfPosAE.first)) / fPeriod;
                    vectTargetInfo[i].pairfPredSpdAE.second = MedianFloat3((vectTargetInfo[i].vectInfoInFrame[iSize - 1].blobMeasure.pairfPosAE.second - vectTargetInfo[i].vectInfoInFrame[iSize - 2].blobMeasure.pairfPosAE.second),
                            (vectTargetInfo[i].vectInfoInFrame[iSize - 2].blobMeasure.pairfPosAE.second - vectTargetInfo[i].vectInfoInFrame[iSize - 3].blobMeasure.pairfPosAE.second),
                            (vectTargetInfo[i].vectInfoInFrame[iSize - 3].blobMeasure.pairfPosAE.second - vectTargetInfo[i].vectInfoInFrame[iSize - 4].blobMeasure.pairfPosAE.second)) / fPeriod;
                    vectTargetInfo[i].pairfPredPosAE.first = vectTargetInfo[i].vectInfoInFrame[iSize - 1].blobMeasure.pairfPosAE.first + vectTargetInfo[i].pairfPredSpdAE.first * fPeriod;
                    vectTargetInfo[i].pairfPredPosAE.second = vectTargetInfo[i].vectInfoInFrame[iSize - 1].blobMeasure.pairfPosAE.second + vectTargetInfo[i].pairfPredSpdAE.second * fPeriod;
                    vectTargetInfo[i].fValid = ((iSize - 1) * vectTargetInfo[i].fValid + (vectTargetInfo[i].vectInfoInFrame[iSize - 1].bValid ? 1 : 0)) / iSize;

    //                if (vectTargetInfo[i].vectInfoInFrame.size() <= 6)
    //                {
    //                    float fDX = vectTargetInfo[i].vectInfoInFrame[vectTargetInfo[i].vectInfoInFrame.size()-1].blobMeasure.pairfPos.first - vectTargetInfo[i].vectInfoInFrame[vectTargetInfo[i].vectInfoInFrame.size()-2].blobMeasure.pairfPos.first;
    //                    float fDY = vectTargetInfo[i].vectInfoInFrame[vectTargetInfo[i].vectInfoInFrame.size()-1].blobMeasure.pairfPos.second - vectTargetInfo[i].vectInfoInFrame[vectTargetInfo[i].vectInfoInFrame.size()-2].blobMeasure.pairfPos.second;
    //                    float fDX1 = vectTargetInfo[i].vectInfoInFrame[vectTargetInfo[i].vectInfoInFrame.size()-2].blobMeasure.pairfPos.first - vectTargetInfo[i].vectInfoInFrame[vectTargetInfo[i].vectInfoInFrame.size()-3].blobMeasure.pairfPos.first;
    //                    float fDY1 = vectTargetInfo[i].vectInfoInFrame[vectTargetInfo[i].vectInfoInFrame.size()-2].blobMeasure.pairfPos.second - vectTargetInfo[i].vectInfoInFrame[vectTargetInfo[i].vectInfoInFrame.size()-3].blobMeasure.pairfPos.second;
    //                    vectTargetInfo[i].bLiving = !(abs(fDX - pairfStarSpd.first) < 3 && abs(fDY - pairfStarSpd.second) < 3)
    //                            && (abs(fDX - fDX1) < 3 && abs(fDY - fDY1) < 3);
    //                }

                    vectTargetInfo[i].bLiving = vectTargetInfo[i].bLiving && (vectTargetInfo[i].pairfPredPosInFrame.first > 0 && vectTargetInfo[i].pairfPredPosInFrame.first < opticparams.iImageWidth
                        && vectTargetInfo[i].pairfPredPosInFrame.second > 0 && vectTargetInfo[i].pairfPredPosInFrame.second < opticparams.iImageHeight);

                    iNumCheck = vectTargetInfo[i].vectInfoInFrame.size() > 20 ? 20 : vectTargetInfo[i].vectInfoInFrame.size();
                    iNumUnValid = 0;
                    for (int j = 0; j < iNumCheck; j++)
                    {
                        iNumUnValid += (!vectTargetInfo[i].vectInfoInFrame[vectTargetInfo[i].vectInfoInFrame.size()-(j+1)].bValid) ? 1 : 0;
                    }
                    vectTargetInfo[i].bLiving = vectTargetInfo[i].bLiving && !(iNumUnValid == iNumCheck);
                }
            }
        }
	}
	else
	{
		return 1;
	}

	return 0;
}

int TrackAlgo::TrackProc_LEO(sMeasuresInFrame measures, sTargetInfo& targetInfo)
{
    if (m_iMode != TRACK_LEO)
    {
        return 1;
    }

    m_vectFIFO_Target.push_back(measures);
    if (!m_bTargetFind)
    {
        InitTargetInfo();
        double dPeriod = 1.0;
        CalcDiffTime(m_vectFIFO_Target[0].stimeFrame, m_vectFIFO_Target[1].stimeFrame, dPeriod);
        m_fFrameFreq = 1.0 / dPeriod;
        if (m_vectFIFO_Target[m_vectFIFO_Target.size()-1].vectTargetMeasures.size() < 2000)
        {
            LEO_FindTargets(m_vectFIFO_Target, m_vectTargetInfoFind, m_settings.fSpdLow_AE, m_settings.fSpdHigh_AE, m_settings.fThreash_AE, m_settings.opticparams);
        }
    }
    else if (!m_bTargetVerify)
    {
        LEO_VerifyTarget(m_vectFIFO_Target, m_vectTargetInfoFind, targetInfo, m_settings.fThreash_AE, m_settings.opticparams, m_settings.fThreashLiving, m_settings.iNumFramesLiving);
    }
    else
    {
        LEO_TrackTarget(m_vectFIFO_Target, targetInfo, m_settings.fThreash_AE, m_settings.opticparams, m_settings.fThreashLiving, m_settings.iNumFramesLiving, m_settings.bAutoDecide);
    }
    m_ulFrameSeq++;

    if (m_vectFIFO_Target.size() > 3)
    {
        vector<sMeasuresInFrame>::iterator k = m_vectFIFO_Target.begin();
        m_vectFIFO_Target.erase(k);	// 删除第一个元素
    }

    m_targetInfo = targetInfo;

    return 0;
}

int TrackAlgo::LEO_Assoc3(vector<sMeasuresInFrame> vectMeasuresInputFIFO, vector<sTargetInfo>& vectTargetInfo, float fSpdLow_AE, float fSpdHigh_AE, float fThresh_AE, sOpticParams opticparams)
{
	double dStart = omp_get_wtime();

	int iSize = vectMeasuresInputFIFO.size();
	if (iSize >= 3)
	{
		if (vectMeasuresInputFIFO[iSize - 3].vectTargetMeasures.size() > 0 && vectMeasuresInputFIFO[iSize - 2].vectTargetMeasures.size() > 0 && vectMeasuresInputFIFO[iSize - 1].vectTargetMeasures.size() > 0)
		{
			vector<vector<sTargetInfo>> vectTargetInfo_temp;
			for (int i = 0; i < m_iNumCPUCores; i++)
			{
				vector<sTargetInfo> vect00;
				vectTargetInfo_temp.push_back(vect00);
			}

#pragma omp parallel for num_threads(m_iNumCPUCores)
			for (int i = 0; i < vectMeasuresInputFIFO[iSize - 3].vectTargetMeasures.size(); i++)
			{
				for (int j = 0; j < vectMeasuresInputFIFO[iSize - 2].vectTargetMeasures.size(); j++)
				{
					float fDx1_0 = vectMeasuresInputFIFO[iSize - 2].vectTargetMeasures.at(j).pairfPosAE.first - vectMeasuresInputFIFO[iSize - 3].vectTargetMeasures.at(i).pairfPosAE.first;
					float fDy1_0 = vectMeasuresInputFIFO[iSize - 2].vectTargetMeasures.at(j).pairfPosAE.second - vectMeasuresInputFIFO[iSize - 3].vectTargetMeasures.at(i).pairfPosAE.second;
                    float fSpdAbs1_0 = pow(pow(fDx1_0, 2.0) + pow(fDy1_0, 2.0), 0.5);
					if (fSpdAbs1_0 > fSpdLow_AE)	// 只要大于恒星运动速度即可,不必限制最高速度
					{
						for (int k = 0; k < vectMeasuresInputFIFO[iSize - 1].vectTargetMeasures.size(); k++)
						{
							float fDx2_1 = vectMeasuresInputFIFO[iSize - 1].vectTargetMeasures.at(k).pairfPosAE.first - vectMeasuresInputFIFO[iSize - 2].vectTargetMeasures.at(j).pairfPosAE.first;
							float fDy2_1 = vectMeasuresInputFIFO[iSize - 1].vectTargetMeasures.at(k).pairfPosAE.second - vectMeasuresInputFIFO[iSize - 2].vectTargetMeasures.at(j).pairfPosAE.second;
                            if (abs(fDx2_1 - fDx1_0) <= fThresh_AE
								&& abs(fDy2_1 - fDy1_0) <= fThresh_AE)
							{
								sTargetInfoInFrame infoinframe1, infoinframe2, infoinframe3;
								infoinframe1.stimeFrame = vectMeasuresInputFIFO[iSize - 3].stimeFrame;
								infoinframe1.ulFrameSeq = vectMeasuresInputFIFO[iSize - 3].ulFrameSeq;
								infoinframe1.blobMeasure = vectMeasuresInputFIFO[iSize - 3].vectTargetMeasures.at(i);
                                infoinframe1.bValid = true;

								infoinframe2.stimeFrame = vectMeasuresInputFIFO[iSize - 2].stimeFrame;
								infoinframe2.ulFrameSeq = vectMeasuresInputFIFO[iSize - 2].ulFrameSeq;
								infoinframe2.blobMeasure = vectMeasuresInputFIFO[iSize - 2].vectTargetMeasures.at(j);
                                infoinframe2.bValid = true;

								infoinframe3.stimeFrame = vectMeasuresInputFIFO[iSize - 1].stimeFrame;
								infoinframe3.ulFrameSeq = vectMeasuresInputFIFO[iSize - 1].ulFrameSeq;
								infoinframe3.blobMeasure = vectMeasuresInputFIFO[iSize - 1].vectTargetMeasures.at(k);
                                infoinframe3.bValid = true;

								sTargetInfo infoAssoc;
								infoAssoc.vectInfoInFrame.push_back(infoinframe1);
								infoAssoc.vectInfoInFrame.push_back(infoinframe2);
								infoAssoc.vectInfoInFrame.push_back(infoinframe3);
                                infoAssoc.pairfPredSpdInFrame.first = ((infoinframe2.blobMeasure.pairfPos.first - infoinframe1.blobMeasure.pairfPos.first) + (infoinframe3.blobMeasure.pairfPos.first - infoinframe2.blobMeasure.pairfPos.first)) / 2.0;
								infoAssoc.pairfPredSpdInFrame.second = ((infoinframe2.blobMeasure.pairfPos.second - infoinframe1.blobMeasure.pairfPos.second) + (infoinframe3.blobMeasure.pairfPos.second - infoinframe2.blobMeasure.pairfPos.second)) / 2.0;
								infoAssoc.pairfPredPosInFrame.first = infoinframe3.blobMeasure.pairfPos.first + infoAssoc.pairfPredSpdInFrame.first;
								infoAssoc.pairfPredPosInFrame.second = infoinframe3.blobMeasure.pairfPos.second + infoAssoc.pairfPredSpdInFrame.second;
                                float fPeriod = 1.0 / m_fFrameFreq;
								infoAssoc.pairfPredSpdAE.first = ((infoinframe2.blobMeasure.pairfPosAE.first - infoinframe1.blobMeasure.pairfPosAE.first) + (infoinframe3.blobMeasure.pairfPosAE.first - infoinframe2.blobMeasure.pairfPosAE.first)) / fPeriod / 2.0;
								infoAssoc.pairfPredSpdAE.second = ((infoinframe2.blobMeasure.pairfPosAE.second - infoinframe1.blobMeasure.pairfPosAE.second) + (infoinframe3.blobMeasure.pairfPosAE.second - infoinframe2.blobMeasure.pairfPosAE.second)) / fPeriod / 2.0;
								infoAssoc.pairfPredPosAE.first = infoinframe3.blobMeasure.pairfPosAE.first + infoAssoc.pairfPredSpdAE.first * fPeriod;
								infoAssoc.pairfPredPosAE.second = infoinframe3.blobMeasure.pairfPosAE.second + infoAssoc.pairfPredSpdAE.second * fPeriod;
								infoAssoc.fValid = 1.0;
								infoAssoc.bLiving = true;

								vectTargetInfo_temp[omp_get_thread_num()].push_back(infoAssoc);
							}
						}
					}
				}
			}
			for (int i = 0; i < m_iNumCPUCores; i++)
			{
				vectTargetInfo.insert(vectTargetInfo.end(), vectTargetInfo_temp[i].begin(), vectTargetInfo_temp[i].end());
			}
		}
		else
		{
			return 2;
		}
	}
	else
	{
		return 1;
	}

	double dEnd = omp_get_wtime();
	//	printf("Assoc3 using time: %.3fs.\n", dEnd - dStart);

	return 0;
}

int TrackAlgo::LEO_FindTargets(vector<sMeasuresInFrame> vectMeasuresInputFIFO, vector<sTargetInfo>& vectTargetInfo, float fSpdLow_AE, float fSpdHigh_AE, float fThresh_AE, sOpticParams opticparams)
{
	int iFIFOSize = vectMeasuresInputFIFO.size();
	if (iFIFOSize >= 3)
	{
		/// 三帧关联
		vector<sTargetInfo> vectTargetInfoTemp;
		if (LEO_Assoc3(vectMeasuresInputFIFO, vectTargetInfoTemp, fSpdLow_AE, fSpdHigh_AE, fThresh_AE, opticparams) == 0)
		{
			vectTargetInfo = vectTargetInfoTemp;
			if (vectTargetInfo.size() > 0)
			{
				m_bTargetFind = true;
			}
		}
		else
		{
			return 2;
		}
	}
	else
	{
		return 1;
	}

	return 0;
}

int TrackAlgo::LEO_VerifyTarget(vector<sMeasuresInFrame> vectMeasuresInputFIFO, vector<sTargetInfo>& vectTargetInfo, sTargetInfo& targetInfo, float fThresh_AE, sOpticParams opticparams, float fThreashLiving, int iNumFramesLiving)
{
	if (vectMeasuresInputFIFO.size() >= 3 && vectTargetInfo.size() > 0)
	{
		bool bLivingTotal = false;
		for (int i = 0; i < vectTargetInfo.size(); i++)
		{
			bLivingTotal |= vectTargetInfo[i].bLiving;
		}
		if (bLivingTotal)
		{
			for (int i = 0; i < vectTargetInfo.size(); i++)
			{
				if (vectTargetInfo[i].bLiving)
				{
					vector<sMeasureBlob> blobSelectTemp;
					vector<float> fDispTempTemp;
					for (int j = 0; j < m_iNumCPUCores; j++)
					{
						sMeasureBlob blob;
						blobSelectTemp.push_back(blob);
						float fDistTemp = 1000000;
						fDispTempTemp.push_back(fDistTemp);
					}

#pragma omp parallel for num_threads(m_iNumCPUCores)
					for (int j = 0; j < vectMeasuresInputFIFO.at(vectMeasuresInputFIFO.size() - 1).vectTargetMeasures.size(); j++)
					{
						float fDistX = vectTargetInfo.at(i).pairfPredPosAE.first - vectMeasuresInputFIFO.at(vectMeasuresInputFIFO.size() - 1).vectTargetMeasures.at(j).pairfPosAE.first;
						float fDistY = vectTargetInfo.at(i).pairfPredPosAE.second - vectMeasuresInputFIFO.at(vectMeasuresInputFIFO.size() - 1).vectTargetMeasures.at(j).pairfPosAE.second;
                        float fDist = pow(pow(fDistX, 2.0) + pow(fDistY, 2.0), 0.5);
                        if (abs(fDistX) < fThresh_AE && abs(fDistY) < fThresh_AE
                                && fDist <= fDispTempTemp[omp_get_thread_num()])
						{
							blobSelectTemp[omp_get_thread_num()] = vectMeasuresInputFIFO.at(vectMeasuresInputFIFO.size() - 1).vectTargetMeasures.at(j);
							fDispTempTemp[omp_get_thread_num()] = fDist;
						}
					}

					sMeasureBlob blobSelect;
					float fDistTemp = 1000000;
					for (int j = 0; j < m_iNumCPUCores; j++)
					{
						if (fDispTempTemp[j] < fDistTemp)
						{
							blobSelect = blobSelectTemp[j];
							fDistTemp = fDispTempTemp[j];
						}
					}

					sTargetInfoInFrame infoTrack;
                    if (fDistTemp < 1000000
                            && abs(vectTargetInfo.at(i).pairfPredPosAE.first - blobSelect.pairfPosAE.first) < fThresh_AE
                            && abs(vectTargetInfo.at(i).pairfPredPosAE.second - blobSelect.pairfPosAE.second) < fThresh_AE)
					{
						infoTrack.stimeFrame = vectMeasuresInputFIFO.at(vectMeasuresInputFIFO.size() - 1).stimeFrame;
						infoTrack.ulFrameSeq = vectMeasuresInputFIFO.at(vectMeasuresInputFIFO.size() - 1).ulFrameSeq;
						infoTrack.blobMeasure = blobSelect;
						infoTrack.bValid = true;
					}
					else
					{
						infoTrack.stimeFrame = vectMeasuresInputFIFO.at(vectMeasuresInputFIFO.size() - 1).stimeFrame;
						infoTrack.ulFrameSeq = vectMeasuresInputFIFO.at(vectMeasuresInputFIFO.size() - 1).ulFrameSeq;
						infoTrack.blobMeasure.pairfPos = vectTargetInfo[i].pairfPredPosInFrame;
						infoTrack.blobMeasure.fMaxX = infoTrack.blobMeasure.pairfPos.first + 5;
						infoTrack.blobMeasure.fMinX = infoTrack.blobMeasure.pairfPos.first - 5;
						infoTrack.blobMeasure.fMaxY = infoTrack.blobMeasure.pairfPos.second + 5;
						infoTrack.blobMeasure.fMinY = infoTrack.blobMeasure.pairfPos.second - 5;
						infoTrack.blobMeasure.fArea = 0;
						infoTrack.blobMeasure.fDN = 0;
						infoTrack.blobMeasure.pairfPosAE = vectTargetInfo[i].pairfPredPosAE;
						infoTrack.bValid = false;
					}
					vectTargetInfo[i].vectInfoInFrame.push_back(infoTrack);
					int iSize = vectTargetInfo[i].vectInfoInFrame.size();
                    vectTargetInfo[i].pairfPredSpdInFrame.first = MedianFloat3((vectTargetInfo[i].vectInfoInFrame[iSize - 1].blobMeasure.pairfPos.first - vectTargetInfo[i].vectInfoInFrame[iSize - 2].blobMeasure.pairfPos.first),
                            (vectTargetInfo[i].vectInfoInFrame[iSize - 2].blobMeasure.pairfPos.first - vectTargetInfo[i].vectInfoInFrame[iSize - 3].blobMeasure.pairfPos.first),
                            (vectTargetInfo[i].vectInfoInFrame[iSize - 3].blobMeasure.pairfPos.first - vectTargetInfo[i].vectInfoInFrame[iSize - 4].blobMeasure.pairfPos.first));
                    vectTargetInfo[i].pairfPredSpdInFrame.second = MedianFloat3((vectTargetInfo[i].vectInfoInFrame[iSize - 1].blobMeasure.pairfPos.second - vectTargetInfo[i].vectInfoInFrame[iSize - 2].blobMeasure.pairfPos.second),
                            (vectTargetInfo[i].vectInfoInFrame[iSize - 2].blobMeasure.pairfPos.second - vectTargetInfo[i].vectInfoInFrame[iSize - 3].blobMeasure.pairfPos.second),
                            (vectTargetInfo[i].vectInfoInFrame[iSize - 3].blobMeasure.pairfPos.second - vectTargetInfo[i].vectInfoInFrame[iSize - 4].blobMeasure.pairfPos.second));
					vectTargetInfo[i].pairfPredPosInFrame.first = vectTargetInfo[i].vectInfoInFrame[iSize - 1].blobMeasure.pairfPos.first + vectTargetInfo[i].pairfPredSpdInFrame.first;
					vectTargetInfo[i].pairfPredPosInFrame.second = vectTargetInfo[i].vectInfoInFrame[iSize - 1].blobMeasure.pairfPos.second + vectTargetInfo[i].pairfPredSpdInFrame.second;
                    float fPeriod = 1.0 / m_fFrameFreq;
                    vectTargetInfo[i].pairfPredSpdAE.first = MedianFloat3((vectTargetInfo[i].vectInfoInFrame[iSize - 1].blobMeasure.pairfPosAE.first - vectTargetInfo[i].vectInfoInFrame[iSize - 2].blobMeasure.pairfPosAE.first),
                            (vectTargetInfo[i].vectInfoInFrame[iSize - 2].blobMeasure.pairfPosAE.first - vectTargetInfo[i].vectInfoInFrame[iSize - 3].blobMeasure.pairfPosAE.first),
                            (vectTargetInfo[i].vectInfoInFrame[iSize - 3].blobMeasure.pairfPosAE.first - vectTargetInfo[i].vectInfoInFrame[iSize - 4].blobMeasure.pairfPosAE.first)) / fPeriod;
                    vectTargetInfo[i].pairfPredSpdAE.second = MedianFloat3((vectTargetInfo[i].vectInfoInFrame[iSize - 1].blobMeasure.pairfPosAE.second - vectTargetInfo[i].vectInfoInFrame[iSize - 2].blobMeasure.pairfPosAE.second),
                            (vectTargetInfo[i].vectInfoInFrame[iSize - 2].blobMeasure.pairfPosAE.second - vectTargetInfo[i].vectInfoInFrame[iSize - 3].blobMeasure.pairfPosAE.second),
                            (vectTargetInfo[i].vectInfoInFrame[iSize - 3].blobMeasure.pairfPosAE.second - vectTargetInfo[i].vectInfoInFrame[iSize - 4].blobMeasure.pairfPosAE.second)) / fPeriod;
					vectTargetInfo[i].pairfPredPosAE.first = vectTargetInfo[i].vectInfoInFrame[iSize - 1].blobMeasure.pairfPosAE.first + vectTargetInfo[i].pairfPredSpdAE.first * fPeriod;
					vectTargetInfo[i].pairfPredPosAE.second = vectTargetInfo[i].vectInfoInFrame[iSize - 1].blobMeasure.pairfPosAE.second + vectTargetInfo[i].pairfPredSpdAE.second * fPeriod;
					vectTargetInfo[i].fValid = ((iSize - 1) * vectTargetInfo[i].fValid + (vectTargetInfo[i].vectInfoInFrame[iSize - 1].bValid ? 1 : 0)) / iSize;
                    if (vectTargetInfo[i].vectInfoInFrame[vectTargetInfo[i].vectInfoInFrame.size()-1].bValid
                            && vectTargetInfo[i].vectInfoInFrame[vectTargetInfo[i].vectInfoInFrame.size()-2].bValid
                            && vectTargetInfo[i].vectInfoInFrame[vectTargetInfo[i].vectInfoInFrame.size()-3].bValid
                            && vectTargetInfo[i].vectInfoInFrame[vectTargetInfo[i].vectInfoInFrame.size()-4].bValid)
                    {
                        float fSpdDiffX3_2 = vectTargetInfo[i].vectInfoInFrame[vectTargetInfo[i].vectInfoInFrame.size()-1].blobMeasure.pairfPosAE.first - vectTargetInfo[i].vectInfoInFrame[vectTargetInfo[i].vectInfoInFrame.size()-2].blobMeasure.pairfPosAE.first;
                        float fSpdDiffX2_1 = vectTargetInfo[i].vectInfoInFrame[vectTargetInfo[i].vectInfoInFrame.size()-2].blobMeasure.pairfPosAE.first - vectTargetInfo[i].vectInfoInFrame[vectTargetInfo[i].vectInfoInFrame.size()-3].blobMeasure.pairfPosAE.first;
                        float fSpdDiffX1_0 = vectTargetInfo[i].vectInfoInFrame[vectTargetInfo[i].vectInfoInFrame.size()-3].blobMeasure.pairfPosAE.first - vectTargetInfo[i].vectInfoInFrame[vectTargetInfo[i].vectInfoInFrame.size()-4].blobMeasure.pairfPosAE.first;
                        float fSpdDiffY3_2 = vectTargetInfo[i].vectInfoInFrame[vectTargetInfo[i].vectInfoInFrame.size()-1].blobMeasure.pairfPosAE.second - vectTargetInfo[i].vectInfoInFrame[vectTargetInfo[i].vectInfoInFrame.size()-2].blobMeasure.pairfPosAE.second;
                        float fSpdDiffY2_1 = vectTargetInfo[i].vectInfoInFrame[vectTargetInfo[i].vectInfoInFrame.size()-2].blobMeasure.pairfPosAE.second - vectTargetInfo[i].vectInfoInFrame[vectTargetInfo[i].vectInfoInFrame.size()-3].blobMeasure.pairfPosAE.second;
                        float fSpdDiffY1_0 = vectTargetInfo[i].vectInfoInFrame[vectTargetInfo[i].vectInfoInFrame.size()-3].blobMeasure.pairfPosAE.second - vectTargetInfo[i].vectInfoInFrame[vectTargetInfo[i].vectInfoInFrame.size()-4].blobMeasure.pairfPosAE.second;

                        int iValid = (abs(fSpdDiffX2_1 - fSpdDiffX1_0) < fThresh_AE ? 1 : 0)
                                + (abs(fSpdDiffX3_2 - fSpdDiffX2_1) < fThresh_AE ? 1 : 0)
                                + (abs(fSpdDiffX3_2 - fSpdDiffX1_0) < fThresh_AE ? 1 : 0)
                                + (abs(fSpdDiffY2_1 - fSpdDiffY1_0) < fThresh_AE ? 1 : 0)
                                + (abs(fSpdDiffY3_2 - fSpdDiffY2_1) < fThresh_AE ? 1 : 0)
                                + (abs(fSpdDiffY3_2 - fSpdDiffY1_0) < fThresh_AE ? 1 : 0);
                        vectTargetInfo[i].bLiving = iValid >= 6;
                    }
                    else
                    {
                        vectTargetInfo[i].bLiving = false;
                    }
				}
            }

			float fSpdTemp = 0;
			sTargetInfo targetInfoTemp;
			for (int i = 0; i < vectTargetInfo.size(); i++)
			{
                if (vectTargetInfo[i].bLiving
                    && vectTargetInfo[i].vectInfoInFrame[vectTargetInfo[i].vectInfoInFrame.size() - 1].bValid)
				{
                    float fSpd = pow(pow(vectTargetInfo[i].pairfPredSpdAE.first, 2) + pow(vectTargetInfo[i].pairfPredSpdAE.second, 2), 0.5);
                    if (fSpd > fSpdTemp)
					{
						targetInfoTemp = vectTargetInfo[i];
						fSpdTemp = fSpd;
					}
				}
			}
			if (fSpdTemp > 0)
			{
				targetInfo = targetInfoTemp;
				m_bTargetVerify = true;
			}
			else
			{
				return 3;
			}
		}
		else
		{
			InitTargetInfo();
			return 2;
		}		
	}
	else
	{
		return 1;
	}

	return 0;
}

int TrackAlgo::LEO_TrackTarget(vector<sMeasuresInFrame> vectMeasuresInputFIFO, sTargetInfo& targetInfo, float fThresh_AE, sOpticParams opticparams, float fThreashLiving, int iNumFramesLiving, bool bAutoDecide)
{
	if (vectMeasuresInputFIFO.size() >= 3)
	{
		if (targetInfo.bLiving)
		{
			vector<sMeasureBlob> blobSelectTemp;
			vector<float> fDispTempTemp;
			for (int j = 0; j < m_iNumCPUCores; j++)
			{
				sMeasureBlob blob;
				blobSelectTemp.push_back(blob);
				float fDistTemp = 1000000;
				fDispTempTemp.push_back(fDistTemp);
			}

#pragma omp parallel for num_threads(m_iNumCPUCores)			
			for (int j = 0; j < vectMeasuresInputFIFO.at(vectMeasuresInputFIFO.size() - 1).vectTargetMeasures.size(); j++)
			{
				float fDistX = targetInfo.pairfPredPosAE.first - vectMeasuresInputFIFO.at(vectMeasuresInputFIFO.size() - 1).vectTargetMeasures.at(j).pairfPosAE.first;
				float fDistY = targetInfo.pairfPredPosAE.second - vectMeasuresInputFIFO.at(vectMeasuresInputFIFO.size() - 1).vectTargetMeasures.at(j).pairfPosAE.second;
                float fDist = pow(pow(fDistX, 2.0) + pow(fDistY, 2.0), 0.5);
                if (fDistX < fThresh_AE && fDistY < fThresh_AE
                        && fDist <= fDispTempTemp[omp_get_thread_num()])
				{
					blobSelectTemp[omp_get_thread_num()] = vectMeasuresInputFIFO.at(vectMeasuresInputFIFO.size() - 1).vectTargetMeasures.at(j);
					fDispTempTemp[omp_get_thread_num()] = fDist;
				}
			}

			sMeasureBlob blobSelect;
			float fDistTemp = 1000000;
			for (int j = 0; j < m_iNumCPUCores; j++)
			{
				if (fDispTempTemp[j] < fDistTemp)
				{
					blobSelect = blobSelectTemp[j];
					fDistTemp = fDispTempTemp[j];
				}
			}

			sTargetInfoInFrame infoTrack;
            if (fDistTemp < 1000000)
			{
				infoTrack.stimeFrame = vectMeasuresInputFIFO.at(vectMeasuresInputFIFO.size() - 1).stimeFrame;
				infoTrack.ulFrameSeq = vectMeasuresInputFIFO.at(vectMeasuresInputFIFO.size() - 1).ulFrameSeq;
				infoTrack.blobMeasure = blobSelect;
				infoTrack.bValid = true;
			}
			else
			{
				infoTrack.stimeFrame = vectMeasuresInputFIFO.at(vectMeasuresInputFIFO.size() - 1).stimeFrame;
				infoTrack.ulFrameSeq = vectMeasuresInputFIFO.at(vectMeasuresInputFIFO.size() - 1).ulFrameSeq;
				infoTrack.blobMeasure.pairfPos = targetInfo.pairfPredPosInFrame;
				infoTrack.blobMeasure.fMaxX = infoTrack.blobMeasure.pairfPos.first + 5;
				infoTrack.blobMeasure.fMinX = infoTrack.blobMeasure.pairfPos.first - 5;
				infoTrack.blobMeasure.fMaxY = infoTrack.blobMeasure.pairfPos.second + 5;
				infoTrack.blobMeasure.fMinY = infoTrack.blobMeasure.pairfPos.second - 5;
				infoTrack.blobMeasure.fArea = 0;
				infoTrack.blobMeasure.fDN = 0;
				infoTrack.blobMeasure.pairfPosAE = targetInfo.pairfPredPosAE;
				infoTrack.bValid = false;
			}
			targetInfo.vectInfoInFrame.push_back(infoTrack);
			int iSize = targetInfo.vectInfoInFrame.size();
            targetInfo.pairfPredSpdInFrame.first = MedianFloat3((targetInfo.vectInfoInFrame[iSize - 1].blobMeasure.pairfPos.first - targetInfo.vectInfoInFrame[iSize - 2].blobMeasure.pairfPos.first),
                    (targetInfo.vectInfoInFrame[iSize - 2].blobMeasure.pairfPos.first - targetInfo.vectInfoInFrame[iSize - 3].blobMeasure.pairfPos.first),
                    (targetInfo.vectInfoInFrame[iSize - 3].blobMeasure.pairfPos.first - targetInfo.vectInfoInFrame[iSize - 4].blobMeasure.pairfPos.first));
            targetInfo.pairfPredSpdInFrame.second = MedianFloat3((targetInfo.vectInfoInFrame[iSize - 1].blobMeasure.pairfPos.second - targetInfo.vectInfoInFrame[iSize - 2].blobMeasure.pairfPos.second),
                    (targetInfo.vectInfoInFrame[iSize - 2].blobMeasure.pairfPos.second - targetInfo.vectInfoInFrame[iSize - 3].blobMeasure.pairfPos.second),
                    (targetInfo.vectInfoInFrame[iSize - 3].blobMeasure.pairfPos.second - targetInfo.vectInfoInFrame[iSize - 4].blobMeasure.pairfPos.second));
			targetInfo.pairfPredPosInFrame.first = targetInfo.vectInfoInFrame[iSize - 1].blobMeasure.pairfPos.first + targetInfo.pairfPredSpdInFrame.first;
			targetInfo.pairfPredPosInFrame.second = targetInfo.vectInfoInFrame[iSize - 1].blobMeasure.pairfPos.second + targetInfo.pairfPredSpdInFrame.second;
            float fPeriod = 1.0 / m_fFrameFreq;
            targetInfo.pairfPredSpdAE.first = MedianFloat3((targetInfo.vectInfoInFrame[iSize - 1].blobMeasure.pairfPosAE.first - targetInfo.vectInfoInFrame[iSize - 2].blobMeasure.pairfPosAE.first),
                    (targetInfo.vectInfoInFrame[iSize - 2].blobMeasure.pairfPosAE.first - targetInfo.vectInfoInFrame[iSize - 3].blobMeasure.pairfPosAE.first),
                    (targetInfo.vectInfoInFrame[iSize - 3].blobMeasure.pairfPosAE.first - targetInfo.vectInfoInFrame[iSize - 4].blobMeasure.pairfPosAE.first)) / fPeriod;
            targetInfo.pairfPredSpdAE.second = MedianFloat3((targetInfo.vectInfoInFrame[iSize - 1].blobMeasure.pairfPosAE.second - targetInfo.vectInfoInFrame[iSize - 2].blobMeasure.pairfPosAE.second),
                    (targetInfo.vectInfoInFrame[iSize - 2].blobMeasure.pairfPosAE.second - targetInfo.vectInfoInFrame[iSize - 3].blobMeasure.pairfPosAE.second),
                    (targetInfo.vectInfoInFrame[iSize - 3].blobMeasure.pairfPosAE.second - targetInfo.vectInfoInFrame[iSize - 4].blobMeasure.pairfPosAE.second)) / fPeriod;
			targetInfo.pairfPredPosAE.first = targetInfo.vectInfoInFrame[iSize - 1].blobMeasure.pairfPosAE.first + targetInfo.pairfPredSpdAE.first * fPeriod;
			targetInfo.pairfPredPosAE.second = targetInfo.vectInfoInFrame[iSize - 1].blobMeasure.pairfPosAE.second + targetInfo.pairfPredSpdAE.second * fPeriod;
			targetInfo.fValid = ((iSize - 1) * targetInfo.fValid + (targetInfo.vectInfoInFrame[iSize - 1].bValid ? 1 : 0)) / iSize;
            targetInfo.bLiving = targetInfo.bLiving
                    && !(!targetInfo.vectInfoInFrame[targetInfo.vectInfoInFrame.size()-1].bValid
                    && !targetInfo.vectInfoInFrame[targetInfo.vectInfoInFrame.size()-2].bValid
                    && !targetInfo.vectInfoInFrame[targetInfo.vectInfoInFrame.size()-3].bValid
                    && !targetInfo.vectInfoInFrame[targetInfo.vectInfoInFrame.size()-4].bValid
                    && !targetInfo.vectInfoInFrame[targetInfo.vectInfoInFrame.size()-5].bValid);
		}
		else
		{
			InitTargetInfo();
			return 2;
		}		
	}
	else
	{
		return 1;
	}

	return 0;
}

int TrackAlgo::TrackProc_SC(sMeasuresInFrame measures, sTargetInfo& targetInfo)
{
    if (m_iMode != TRACK_SC)
    {
        return 1;
    }

    m_vectFIFO_Target.push_back(measures);
    if (!m_bTargetFind)
    {
        InitTargetInfo();
        double dPeriod = 1.0;
        CalcDiffTime(m_vectFIFO_Target[0].stimeFrame, m_vectFIFO_Target[1].stimeFrame, dPeriod);
        m_fFrameFreq = 1.0 / dPeriod;
        SC_FindTargets(m_vectFIFO_Target, m_vectTargetInfoFind, m_settings.fRadius, m_settings.fThreashMEO, m_settings.opticparams);
    }
    else if (!m_bTargetVerify)
    {
        SC_VerifyTarget(m_vectFIFO_Target, m_vectTargetInfoFind, targetInfo, m_settings.fThreashMEO, m_settings.opticparams, m_settings.fThreashLiving, m_settings.iNumFramesLiving);
    }
    else
    {
        SC_TrackTarget(m_vectFIFO_Target, targetInfo, m_settings.fThreashMEO, m_settings.opticparams, m_settings.fThreashLiving, m_settings.iNumFramesLiving, m_settings.bAutoDecide);
    }
    m_ulFrameSeq++;

    if (m_vectFIFO_Target.size() > 3)
    {
        vector<sMeasuresInFrame>::iterator k = m_vectFIFO_Target.begin();
        m_vectFIFO_Target.erase(k);	// 删除第一个元素
    }

    m_targetInfo = targetInfo;

    return 0;
}

int TrackAlgo::SC_Assoc3(vector<sMeasuresInFrame> vectMeasuresInputFIFO, vector<sTargetInfo>& vectTargetInfo, float fRadius, float fThresh, sOpticParams opticparams)
{
    double dStart = omp_get_wtime();

    int iSize = vectMeasuresInputFIFO.size();
    if (iSize >= 3)
    {
        if (vectMeasuresInputFIFO[iSize - 3].vectTargetMeasures.size() > 0 && vectMeasuresInputFIFO[iSize - 2].vectTargetMeasures.size() > 0 && vectMeasuresInputFIFO[iSize - 1].vectTargetMeasures.size() > 0)
        {
            vector<vector<sTargetInfo>> vectTargetInfo_temp;
            for (int i = 0; i < m_iNumCPUCores; i++)
            {
                vector<sTargetInfo> vect00;
                vectTargetInfo_temp.push_back(vect00);
            }

#pragma omp parallel for num_threads(m_iNumCPUCores)
            for (int i = 0; i < vectMeasuresInputFIFO[iSize - 3].vectTargetMeasures.size(); i++)
            {
                for (int j = 0; j < vectMeasuresInputFIFO[iSize - 2].vectTargetMeasures.size(); j++)
                {
                    float fDx1_0 = vectMeasuresInputFIFO[iSize - 2].vectTargetMeasures.at(j).pairfPos.first - vectMeasuresInputFIFO[iSize - 3].vectTargetMeasures.at(i).pairfPos.first;
                    float fDy1_0 = vectMeasuresInputFIFO[iSize - 2].vectTargetMeasures.at(j).pairfPos.second - vectMeasuresInputFIFO[iSize - 3].vectTargetMeasures.at(i).pairfPos.second;
                    if (abs(fDx1_0) < fRadius && abs(fDy1_0) < fRadius
                            && vectMeasuresInputFIFO[iSize - 2].vectTargetMeasures.at(j).pairfPos.first > opticparams.fFOVCenterX - 128
                            && vectMeasuresInputFIFO[iSize - 2].vectTargetMeasures.at(j).pairfPos.first < opticparams.fFOVCenterX + 128
                            && vectMeasuresInputFIFO[iSize - 2].vectTargetMeasures.at(j).pairfPos.second > opticparams.fFOVCenterY - 128
                            && vectMeasuresInputFIFO[iSize - 2].vectTargetMeasures.at(j).pairfPos.second < opticparams.fFOVCenterY + 128)
                    {
                        for (int k = 0; k < vectMeasuresInputFIFO[iSize - 1].vectTargetMeasures.size(); k++)
                        {
                            float fDx2_1 = vectMeasuresInputFIFO[iSize - 1].vectTargetMeasures.at(k).pairfPos.first - vectMeasuresInputFIFO[iSize - 2].vectTargetMeasures.at(j).pairfPos.first;
                            float fDy2_1 = vectMeasuresInputFIFO[iSize - 1].vectTargetMeasures.at(k).pairfPos.second - vectMeasuresInputFIFO[iSize - 2].vectTargetMeasures.at(j).pairfPos.second;
                            if (abs(fDx2_1 - fDx1_0) <= fThresh && abs(fDy2_1 - fDy1_0) <= fThresh)
                            {
                                sTargetInfoInFrame infoinframe1, infoinframe2, infoinframe3;
                                infoinframe1.stimeFrame = vectMeasuresInputFIFO[iSize - 3].stimeFrame;
                                infoinframe1.ulFrameSeq = vectMeasuresInputFIFO[iSize - 3].ulFrameSeq;
                                infoinframe1.blobMeasure = vectMeasuresInputFIFO[iSize - 3].vectTargetMeasures.at(i);
                                infoinframe1.bValid = true;

                                infoinframe2.stimeFrame = vectMeasuresInputFIFO[iSize - 2].stimeFrame;
                                infoinframe2.ulFrameSeq = vectMeasuresInputFIFO[iSize - 2].ulFrameSeq;
                                infoinframe2.blobMeasure = vectMeasuresInputFIFO[iSize - 2].vectTargetMeasures.at(j);
                                infoinframe2.bValid = true;

                                infoinframe3.stimeFrame = vectMeasuresInputFIFO[iSize - 1].stimeFrame;
                                infoinframe3.ulFrameSeq = vectMeasuresInputFIFO[iSize - 1].ulFrameSeq;
                                infoinframe3.blobMeasure = vectMeasuresInputFIFO[iSize - 1].vectTargetMeasures.at(k);
                                infoinframe3.bValid = true;

                                sTargetInfo infoAssoc;
                                infoAssoc.vectInfoInFrame.push_back(infoinframe1);
                                infoAssoc.vectInfoInFrame.push_back(infoinframe2);
                                infoAssoc.vectInfoInFrame.push_back(infoinframe3);
                                infoAssoc.pairfPredSpdInFrame.first = ((infoinframe2.blobMeasure.pairfPos.first - infoinframe1.blobMeasure.pairfPos.first) + (infoinframe3.blobMeasure.pairfPos.first - infoinframe2.blobMeasure.pairfPos.first)) / 2.0;
                                infoAssoc.pairfPredSpdInFrame.second = ((infoinframe2.blobMeasure.pairfPos.second - infoinframe1.blobMeasure.pairfPos.second) + (infoinframe3.blobMeasure.pairfPos.second - infoinframe2.blobMeasure.pairfPos.second)) / 2.0;
                                infoAssoc.pairfPredPosInFrame.first = infoinframe3.blobMeasure.pairfPos.first + infoAssoc.pairfPredSpdInFrame.first;
                                infoAssoc.pairfPredPosInFrame.second = infoinframe3.blobMeasure.pairfPos.second + infoAssoc.pairfPredSpdInFrame.second;
                                float fPeriod = 1.0 / m_fFrameFreq;
                                infoAssoc.pairfPredSpdAE.first = ((infoinframe2.blobMeasure.pairfPosAE.first - infoinframe1.blobMeasure.pairfPosAE.first) + (infoinframe3.blobMeasure.pairfPosAE.first - infoinframe2.blobMeasure.pairfPosAE.first)) / fPeriod / 2.0;
                                infoAssoc.pairfPredSpdAE.second = ((infoinframe2.blobMeasure.pairfPosAE.second - infoinframe1.blobMeasure.pairfPosAE.second) + (infoinframe3.blobMeasure.pairfPosAE.second - infoinframe2.blobMeasure.pairfPosAE.second)) / fPeriod / 2.0;
                                infoAssoc.pairfPredPosAE.first = infoinframe3.blobMeasure.pairfPosAE.first + infoAssoc.pairfPredSpdAE.first * fPeriod;
                                infoAssoc.pairfPredPosAE.second = infoinframe3.blobMeasure.pairfPosAE.second + infoAssoc.pairfPredSpdAE.second * fPeriod;
                                infoAssoc.fValid = 1.0;
                                infoAssoc.bLiving = true;

                                vectTargetInfo_temp[omp_get_thread_num()].push_back(infoAssoc);
                            }
                        }
                    }
                }
            }
            for (int i = 0; i < m_iNumCPUCores; i++)
            {
                vectTargetInfo.insert(vectTargetInfo.end(), vectTargetInfo_temp[i].begin(), vectTargetInfo_temp[i].end());
            }

            /// 消除相似轨迹(所有在fThresh范围内的点及后续航迹均被压缩为1条)
            vector<int> vectSeqRemove;
            vector<vector<int>> vectSeqRemove_temp;
            for (int i = 0; i < m_iNumCPUCores; i++)
            {
                vector<int> vect00;
                vectSeqRemove_temp.push_back(vect00);
            }

#pragma omp parallel for num_threads(m_iNumCPUCores)
            for (int i = 0; i < vectTargetInfo.size(); i++)
            {
                for (int j = i + 1; j < vectTargetInfo.size(); j++)	// j = i + 1保证了单向搜索
                {
                    int iSame0 = ((vectTargetInfo.at(i).vectInfoInFrame[0].blobMeasure.pairfPos.first == vectTargetInfo.at(j).vectInfoInFrame[0].blobMeasure.pairfPos.first) && (vectTargetInfo.at(i).vectInfoInFrame[0].blobMeasure.pairfPos.second == vectTargetInfo.at(j).vectInfoInFrame[0].blobMeasure.pairfPos.second)) ? 1 : 0;
                    int iSame1 = ((vectTargetInfo.at(i).vectInfoInFrame[1].blobMeasure.pairfPos.first == vectTargetInfo.at(j).vectInfoInFrame[1].blobMeasure.pairfPos.first) && (vectTargetInfo.at(i).vectInfoInFrame[1].blobMeasure.pairfPos.second == vectTargetInfo.at(j).vectInfoInFrame[1].blobMeasure.pairfPos.second)) ? 1 : 0;
                    int iSame2 = ((vectTargetInfo.at(i).vectInfoInFrame[2].blobMeasure.pairfPos.first == vectTargetInfo.at(j).vectInfoInFrame[2].blobMeasure.pairfPos.first) && (vectTargetInfo.at(i).vectInfoInFrame[2].blobMeasure.pairfPos.second == vectTargetInfo.at(j).vectInfoInFrame[2].blobMeasure.pairfPos.second)) ? 1 : 0;
                    if (iSame0 + iSame1 + iSame2 >= 1)
                    {
                        vectSeqRemove_temp[omp_get_thread_num()].push_back(j);
                    }
                }
            }
            for (int i = 0; i < m_iNumCPUCores; i++)
            {
                vectSeqRemove.insert(vectSeqRemove.end(), vectSeqRemove_temp[i].begin(), vectSeqRemove_temp[i].end());
            }
            if (vectSeqRemove.size() > 0)
            {
                BubbleSortReverse(vectSeqRemove);
                if (vectSeqRemove.size() > 0)
                {
                    vectSeqRemove.erase(unique(vectSeqRemove.begin(), vectSeqRemove.end()), vectSeqRemove.end());
                }
                for (int i = 0; i < vectSeqRemove.size(); i++)
                {
                    if (vectTargetInfo.size() > 0)
                    {
                        vectTargetInfo.erase(vectTargetInfo.begin() + vectSeqRemove[i]);
                    }
                }
            }
        }
        else
        {
            return 2;
        }
    }
    else
    {
        return 1;
    }

    double dEnd = omp_get_wtime();
    //	printf("Assoc3 using time: %.3fs.\n", dEnd - dStart);

    return 0;
}

int TrackAlgo::SC_FindTargets(vector<sMeasuresInFrame> vectMeasuresInputFIFO, vector<sTargetInfo>& vectTargetInfo, float fRadius, float fThresh, sOpticParams opticparams)
{
    int iFIFOSize = vectMeasuresInputFIFO.size();
    if (iFIFOSize >= 3)
    {
        /// 三帧关联
        vector<sTargetInfo> vectTargetInfoTemp;
        if (SC_Assoc3(vectMeasuresInputFIFO, vectTargetInfoTemp, fRadius, fThresh, opticparams) == 0)
        {
            vectTargetInfo = vectTargetInfoTemp;
            if (vectTargetInfo.size() > 0)
            {
                m_bTargetFind = true;
            }
        }
        else
        {
            return 2;
        }
    }
    else
    {
        return 1;
    }

    return 0;
}

int TrackAlgo::SC_VerifyTarget(vector<sMeasuresInFrame> vectMeasuresInputFIFO, vector<sTargetInfo>& vectTargetInfo, sTargetInfo& targetInfo, float fThresh, sOpticParams opticparams, float fThreashLiving, int iNumFramesLiving)
{
    if (vectMeasuresInputFIFO.size() >= 3 && vectTargetInfo.size() > 0)
    {
        bool bLivingTotal = false;
        for (int i = 0; i < vectTargetInfo.size(); i++)
        {
            bLivingTotal |= vectTargetInfo[i].bLiving;
        }
        if (bLivingTotal)
        {
            for (int i = 0; i < vectTargetInfo.size(); i++)
            {
                if (vectTargetInfo[i].bLiving)
                {
                    vector<sMeasureBlob> blobSelectTemp;
                    vector<float> fDispTempTemp;
                    for (int j = 0; j < m_iNumCPUCores; j++)
                    {
                        sMeasureBlob blob;
                        blobSelectTemp.push_back(blob);
                        float fDistTemp = 1000000;
                        fDispTempTemp.push_back(fDistTemp);
                    }

#pragma omp parallel for num_threads(m_iNumCPUCores)
                    for (int j = 0; j < vectMeasuresInputFIFO.at(vectMeasuresInputFIFO.size() - 1).vectTargetMeasures.size(); j++)
                    {
                        float fDistX = vectTargetInfo.at(i).pairfPredPosInFrame.first - vectMeasuresInputFIFO.at(vectMeasuresInputFIFO.size() - 1).vectTargetMeasures.at(j).pairfPos.first;
                        float fDistY = vectTargetInfo.at(i).pairfPredPosInFrame.second - vectMeasuresInputFIFO.at(vectMeasuresInputFIFO.size() - 1).vectTargetMeasures.at(j).pairfPos.second;
                        float fDist = pow(fDistX, 2.0) + pow(fDistY, 2.0);
                        if (abs(fDistX) <= fThresh && abs(fDistY) <= fThresh && fDist <= fDispTempTemp[omp_get_thread_num()])
                        {
                            blobSelectTemp[omp_get_thread_num()] = vectMeasuresInputFIFO.at(vectMeasuresInputFIFO.size() - 1).vectTargetMeasures.at(j);
                            fDispTempTemp[omp_get_thread_num()] = fDist;
                        }
                    }

                    sMeasureBlob blobSelect;
                    float fDistTemp = 1000000;
                    for (int j = 0; j < m_iNumCPUCores; j++)
                    {
                        if (fDispTempTemp[j] < fDistTemp)
                        {
                            blobSelect = blobSelectTemp[j];
                            fDistTemp = fDispTempTemp[j];
                        }
                    }

                    sTargetInfoInFrame infoTrack;
                    if (fDistTemp < 1000000)
                    {
                        infoTrack.stimeFrame = vectMeasuresInputFIFO.at(vectMeasuresInputFIFO.size() - 1).stimeFrame;
                        infoTrack.ulFrameSeq = vectMeasuresInputFIFO.at(vectMeasuresInputFIFO.size() - 1).ulFrameSeq;
                        infoTrack.blobMeasure = blobSelect;
                        infoTrack.bValid = true;
                    }
                    else
                    {
                        infoTrack.stimeFrame = vectMeasuresInputFIFO.at(vectMeasuresInputFIFO.size() - 1).stimeFrame;
                        infoTrack.ulFrameSeq = vectMeasuresInputFIFO.at(vectMeasuresInputFIFO.size() - 1).ulFrameSeq;
                        infoTrack.blobMeasure.pairfPos = vectTargetInfo[i].pairfPredPosInFrame;
                        infoTrack.blobMeasure.fMaxX = infoTrack.blobMeasure.pairfPos.first + 5;
                        infoTrack.blobMeasure.fMinX = infoTrack.blobMeasure.pairfPos.first - 5;
                        infoTrack.blobMeasure.fMaxY = infoTrack.blobMeasure.pairfPos.second + 5;
                        infoTrack.blobMeasure.fMinY = infoTrack.blobMeasure.pairfPos.second - 5;
                        infoTrack.blobMeasure.fArea = 0;
                        infoTrack.blobMeasure.fDN = 0;
                        infoTrack.blobMeasure.pairfPosAE = vectTargetInfo[i].pairfPredPosAE;
                        infoTrack.bValid = false;
                    }
                    vectTargetInfo[i].vectInfoInFrame.push_back(infoTrack);
                    int iSize = vectTargetInfo[i].vectInfoInFrame.size();
                    vectTargetInfo[i].pairfPredSpdInFrame.first = MedianFloat3((vectTargetInfo[i].vectInfoInFrame[iSize - 1].blobMeasure.pairfPos.first - vectTargetInfo[i].vectInfoInFrame[iSize - 2].blobMeasure.pairfPos.first),
                            (vectTargetInfo[i].vectInfoInFrame[iSize - 2].blobMeasure.pairfPos.first - vectTargetInfo[i].vectInfoInFrame[iSize - 3].blobMeasure.pairfPos.first),
                            (vectTargetInfo[i].vectInfoInFrame[iSize - 3].blobMeasure.pairfPos.first - vectTargetInfo[i].vectInfoInFrame[iSize - 4].blobMeasure.pairfPos.first));
                    vectTargetInfo[i].pairfPredSpdInFrame.second = MedianFloat3((vectTargetInfo[i].vectInfoInFrame[iSize - 1].blobMeasure.pairfPos.second - vectTargetInfo[i].vectInfoInFrame[iSize - 2].blobMeasure.pairfPos.second),
                            (vectTargetInfo[i].vectInfoInFrame[iSize - 2].blobMeasure.pairfPos.second - vectTargetInfo[i].vectInfoInFrame[iSize - 3].blobMeasure.pairfPos.second),
                            (vectTargetInfo[i].vectInfoInFrame[iSize - 3].blobMeasure.pairfPos.second - vectTargetInfo[i].vectInfoInFrame[iSize - 4].blobMeasure.pairfPos.second));
                    vectTargetInfo[i].pairfPredPosInFrame.first = vectTargetInfo[i].vectInfoInFrame[iSize - 1].blobMeasure.pairfPos.first + vectTargetInfo[i].pairfPredSpdInFrame.first;
                    vectTargetInfo[i].pairfPredPosInFrame.second = vectTargetInfo[i].vectInfoInFrame[iSize - 1].blobMeasure.pairfPos.second + vectTargetInfo[i].pairfPredSpdInFrame.second;
                    float fPeriod = 1.0 / m_fFrameFreq;
                    vectTargetInfo[i].pairfPredSpdAE.first = MedianFloat3((vectTargetInfo[i].vectInfoInFrame[iSize - 1].blobMeasure.pairfPosAE.first - vectTargetInfo[i].vectInfoInFrame[iSize - 2].blobMeasure.pairfPosAE.first),
                            (vectTargetInfo[i].vectInfoInFrame[iSize - 2].blobMeasure.pairfPosAE.first - vectTargetInfo[i].vectInfoInFrame[iSize - 3].blobMeasure.pairfPosAE.first),
                            (vectTargetInfo[i].vectInfoInFrame[iSize - 3].blobMeasure.pairfPosAE.first - vectTargetInfo[i].vectInfoInFrame[iSize - 4].blobMeasure.pairfPosAE.first)) / fPeriod;
                    vectTargetInfo[i].pairfPredSpdAE.second = MedianFloat3((vectTargetInfo[i].vectInfoInFrame[iSize - 1].blobMeasure.pairfPosAE.second - vectTargetInfo[i].vectInfoInFrame[iSize - 2].blobMeasure.pairfPosAE.second),
                            (vectTargetInfo[i].vectInfoInFrame[iSize - 2].blobMeasure.pairfPosAE.second - vectTargetInfo[i].vectInfoInFrame[iSize - 3].blobMeasure.pairfPosAE.second),
                            (vectTargetInfo[i].vectInfoInFrame[iSize - 3].blobMeasure.pairfPosAE.second - vectTargetInfo[i].vectInfoInFrame[iSize - 4].blobMeasure.pairfPosAE.second)) / fPeriod;
                    vectTargetInfo[i].pairfPredPosAE.first = vectTargetInfo[i].vectInfoInFrame[iSize - 1].blobMeasure.pairfPosAE.first + vectTargetInfo[i].pairfPredSpdAE.first * fPeriod;
                    vectTargetInfo[i].pairfPredPosAE.second = vectTargetInfo[i].vectInfoInFrame[iSize - 1].blobMeasure.pairfPosAE.second + vectTargetInfo[i].pairfPredSpdAE.second * fPeriod;
                    vectTargetInfo[i].fValid = ((iSize - 1) * vectTargetInfo[i].fValid + (vectTargetInfo[i].vectInfoInFrame[iSize - 1].bValid ? 1 : 0)) / iSize;
                    vectTargetInfo[i].bLiving = !(vectTargetInfo[i].vectInfoInFrame.size() >= iNumFramesLiving && vectTargetInfo[i].fValid < fThreashLiving && !vectTargetInfo[i].vectInfoInFrame[vectTargetInfo[i].vectInfoInFrame.size() - 1].bValid);
                }
            }
            /// Find maximum of the blob area
            float fTemp = 0;
            sTargetInfo targetInfoTemp;
            for (int i = 0; i < vectTargetInfo.size(); i++)
            {
                if (vectTargetInfo[i].bLiving)
                {
                    if (vectTargetInfo[i].vectInfoInFrame[vectTargetInfo[i].vectInfoInFrame.size()-1].blobMeasure.fArea > fTemp)
                    {
                        fTemp = vectTargetInfo[i].vectInfoInFrame[vectTargetInfo[i].vectInfoInFrame.size()-1].blobMeasure.fArea;
                        targetInfoTemp = vectTargetInfo[i];
                    }
//                    qDebug() << "Target" << i << "Pos:" << "("
//                             << vectTargetInfo[i].vectInfoInFrame[vectTargetInfo[i].vectInfoInFrame.size()-1].blobMeasure.pairfPos.first
//                             <<  ","
//                             << vectTargetInfo[i].vectInfoInFrame[vectTargetInfo[i].vectInfoInFrame.size()-1].blobMeasure.pairfPos.second
//                             << ")";
                }
            }
            if (fTemp > 0)
            {
                targetInfo = targetInfoTemp;
                m_bTargetVerify = true;
            }
            else
            {
                return 3;
            }
        }
        else
        {
            InitTargetInfo();
            return 2;
        }
    }
    else
    {
        return 1;
    }

    return 0;
}

int TrackAlgo::SC_TrackTarget(vector<sMeasuresInFrame> vectMeasuresInputFIFO, sTargetInfo& targetInfo, float fThresh, sOpticParams opticparams, float fThreashLiving, int iNumFramesLiving, bool bAutoDecide)
{
    if (vectMeasuresInputFIFO.size() >= 3)
    {
        if (targetInfo.bLiving)
        {
            vector<sMeasureBlob> blobSelectTemp;
            vector<float> fDistTempTemp;
            for (int j = 0; j < m_iNumCPUCores; j++)
            {
                sMeasureBlob blob;
                blobSelectTemp.push_back(blob);
                float fDistTemp = 1000000;
                fDistTempTemp.push_back(fDistTemp);
            }

#pragma omp parallel for num_threads(m_iNumCPUCores)
            for (int j = 0; j < vectMeasuresInputFIFO.at(vectMeasuresInputFIFO.size() - 1).vectTargetMeasures.size(); j++)
            {
                float fDistX = targetInfo.pairfPredPosInFrame.first - vectMeasuresInputFIFO.at(vectMeasuresInputFIFO.size() - 1).vectTargetMeasures.at(j).pairfPos.first;
                float fDistY = targetInfo.pairfPredPosInFrame.second - vectMeasuresInputFIFO.at(vectMeasuresInputFIFO.size() - 1).vectTargetMeasures.at(j).pairfPos.second;
                float fDist = pow(fDistX, 2.0) + pow(fDistY, 2.0);
                if (abs(fDistX) <= fThresh && abs(fDistY) <= fThresh && fDist <= fDistTempTemp[omp_get_thread_num()]
                        && vectMeasuresInputFIFO.at(vectMeasuresInputFIFO.size() - 1).vectTargetMeasures.at(j).pairfPos.first < opticparams.fFOVCenterX + 128
                        && vectMeasuresInputFIFO.at(vectMeasuresInputFIFO.size() - 1).vectTargetMeasures.at(j).pairfPos.first > opticparams.fFOVCenterX - 128
                        && vectMeasuresInputFIFO.at(vectMeasuresInputFIFO.size() - 1).vectTargetMeasures.at(j).pairfPos.second < opticparams.fFOVCenterY + 128
                        && vectMeasuresInputFIFO.at(vectMeasuresInputFIFO.size() - 1).vectTargetMeasures.at(j).pairfPos.second > opticparams.fFOVCenterY - 128)
                {
                    blobSelectTemp[omp_get_thread_num()] = vectMeasuresInputFIFO.at(vectMeasuresInputFIFO.size() - 1).vectTargetMeasures.at(j);
                    fDistTempTemp[omp_get_thread_num()] = fDist;
                }
            }

            sMeasureBlob blobSelect;
            float fDistTemp = 1000000;
            for (int j = 0; j < m_iNumCPUCores; j++)
            {
                if (fDistTempTemp[j] < fDistTemp)
                {
                    blobSelect = blobSelectTemp[j];
                    fDistTemp = fDistTempTemp[j];
                }
            }

            sTargetInfoInFrame infoTrack;
            if (fDistTemp < 1000000)
            {
                infoTrack.stimeFrame = vectMeasuresInputFIFO.at(vectMeasuresInputFIFO.size() - 1).stimeFrame;
                infoTrack.ulFrameSeq = vectMeasuresInputFIFO.at(vectMeasuresInputFIFO.size() - 1).ulFrameSeq;
                infoTrack.blobMeasure = blobSelect;
                infoTrack.bValid = true;
            }
            else
            {
                infoTrack.stimeFrame = vectMeasuresInputFIFO.at(vectMeasuresInputFIFO.size() - 1).stimeFrame;
                infoTrack.ulFrameSeq = vectMeasuresInputFIFO.at(vectMeasuresInputFIFO.size() - 1).ulFrameSeq;
                infoTrack.blobMeasure.pairfPos = targetInfo.pairfPredPosInFrame;
                infoTrack.blobMeasure.fMaxX = infoTrack.blobMeasure.pairfPos.first + 5;
                infoTrack.blobMeasure.fMinX = infoTrack.blobMeasure.pairfPos.first - 5;
                infoTrack.blobMeasure.fMaxY = infoTrack.blobMeasure.pairfPos.second + 5;
                infoTrack.blobMeasure.fMinY = infoTrack.blobMeasure.pairfPos.second - 5;
                infoTrack.blobMeasure.fArea = 0;
                infoTrack.blobMeasure.fDN = 0;
                infoTrack.blobMeasure.pairfPosAE = targetInfo.pairfPredPosAE;
                infoTrack.bValid = false;
            }
            targetInfo.vectInfoInFrame.push_back(infoTrack);
            int iSize = targetInfo.vectInfoInFrame.size();
            targetInfo.pairfPredSpdInFrame.first = MedianFloat3((targetInfo.vectInfoInFrame[iSize - 1].blobMeasure.pairfPos.first - targetInfo.vectInfoInFrame[iSize - 2].blobMeasure.pairfPos.first),
                    (targetInfo.vectInfoInFrame[iSize - 2].blobMeasure.pairfPos.first - targetInfo.vectInfoInFrame[iSize - 3].blobMeasure.pairfPos.first),
                    (targetInfo.vectInfoInFrame[iSize - 3].blobMeasure.pairfPos.first - targetInfo.vectInfoInFrame[iSize - 4].blobMeasure.pairfPos.first));
            targetInfo.pairfPredSpdInFrame.second = MedianFloat3((targetInfo.vectInfoInFrame[iSize - 1].blobMeasure.pairfPos.second - targetInfo.vectInfoInFrame[iSize - 2].blobMeasure.pairfPos.second),
                    (targetInfo.vectInfoInFrame[iSize - 2].blobMeasure.pairfPos.second - targetInfo.vectInfoInFrame[iSize - 3].blobMeasure.pairfPos.second),
                    (targetInfo.vectInfoInFrame[iSize - 3].blobMeasure.pairfPos.second - targetInfo.vectInfoInFrame[iSize - 4].blobMeasure.pairfPos.second));
            targetInfo.pairfPredPosInFrame.first = targetInfo.vectInfoInFrame[iSize - 1].blobMeasure.pairfPos.first + targetInfo.pairfPredSpdInFrame.first;
            targetInfo.pairfPredPosInFrame.second = targetInfo.vectInfoInFrame[iSize - 1].blobMeasure.pairfPos.second + targetInfo.pairfPredSpdInFrame.second;
            float fPeriod = 1.0 / m_fFrameFreq;
            targetInfo.pairfPredSpdAE.first = MedianFloat3((targetInfo.vectInfoInFrame[iSize - 1].blobMeasure.pairfPosAE.first - targetInfo.vectInfoInFrame[iSize - 2].blobMeasure.pairfPosAE.first),
                    (targetInfo.vectInfoInFrame[iSize - 2].blobMeasure.pairfPosAE.first - targetInfo.vectInfoInFrame[iSize - 3].blobMeasure.pairfPosAE.first),
                    (targetInfo.vectInfoInFrame[iSize - 3].blobMeasure.pairfPosAE.first - targetInfo.vectInfoInFrame[iSize - 4].blobMeasure.pairfPosAE.first)) / fPeriod;
            targetInfo.pairfPredSpdAE.second = MedianFloat3((targetInfo.vectInfoInFrame[iSize - 1].blobMeasure.pairfPosAE.second - targetInfo.vectInfoInFrame[iSize - 2].blobMeasure.pairfPosAE.second),
                    (targetInfo.vectInfoInFrame[iSize - 2].blobMeasure.pairfPosAE.second - targetInfo.vectInfoInFrame[iSize - 3].blobMeasure.pairfPosAE.second),
                    (targetInfo.vectInfoInFrame[iSize - 3].blobMeasure.pairfPosAE.second - targetInfo.vectInfoInFrame[iSize - 4].blobMeasure.pairfPosAE.second)) / fPeriod;
            targetInfo.pairfPredPosAE.first = targetInfo.vectInfoInFrame[iSize - 1].blobMeasure.pairfPosAE.first + targetInfo.pairfPredSpdAE.first * fPeriod;
            targetInfo.pairfPredPosAE.second = targetInfo.vectInfoInFrame[iSize - 1].blobMeasure.pairfPosAE.second + targetInfo.pairfPredSpdAE.second * fPeriod;
            targetInfo.fValid = ((iSize - 1) * targetInfo.fValid + (targetInfo.vectInfoInFrame[iSize - 1].bValid ? 1 : 0)) / iSize;
//            if (bAutoDecide)
//            {
//                targetInfo.bLiving = !(targetInfo.vectInfoInFrame.size() >= iNumFramesLiving && targetInfo.fValid < fThreashLiving && !targetInfo.vectInfoInFrame[targetInfo.vectInfoInFrame.size() - 1].bValid);
//            }
            targetInfo.bLiving = targetInfo.vectInfoInFrame[targetInfo.vectInfoInFrame.size() - 1].bValid;
        }
        else
        {
            InitTargetInfo();
            return 2;
        }
    }
    else
    {
        return 1;
    }

    return 0;
}

int TrackAlgo::TrackProc_MANUAL(sMeasuresInFrame measures, sTargetInfo& targetInfo, const sMeasureBlob& ManualBlob)
{
    if (m_iMode != TRACK_MANUAL)
    {
        return 1;
    }

    m_vectFIFO_Target.push_back(measures);
    if (!m_bTargetFind)
    {
        InitTargetInfo();
        double dPeriod = 1.0;
        CalcDiffTime(m_vectFIFO_Target[0].stimeFrame, m_vectFIFO_Target[1].stimeFrame, dPeriod);
        m_fFrameFreq = 1.0 / dPeriod;
        MANUAL_FindTargets(m_vectFIFO_Target, m_vectTargetInfoFind, m_settings.fRadius, m_settings.fThreashMEO, m_settings.opticparams, ManualBlob);
    }
    else if (!m_bTargetVerify)
    {
        MANUAL_VerifyTarget(m_vectFIFO_Target, m_vectTargetInfoFind, targetInfo, m_settings.fThreashMEO, m_settings.opticparams, m_settings.fThreashLiving, m_settings.iNumFramesLiving, ManualBlob);
    }
    else
    {
        MANUAL_TrackTarget(m_vectFIFO_Target, targetInfo, m_settings.fThreashMEO, m_settings.opticparams, m_settings.fThreashLiving, m_settings.iNumFramesLiving, m_settings.bAutoDecide, ManualBlob);
    }
    m_ulFrameSeq++;

    if (m_vectFIFO_Target.size() > 3)
    {
        vector<sMeasuresInFrame>::iterator k = m_vectFIFO_Target.begin();
        m_vectFIFO_Target.erase(k);	// 删除第一个元素
    }

    m_targetInfo = targetInfo;

    return 0;
}

int TrackAlgo::MANUAL_Assoc3(vector<sMeasuresInFrame> vectMeasuresInputFIFO, vector<sTargetInfo>& vectTargetInfo, float fRadius, float fThresh, sOpticParams opticparams)
{
    double dStart = omp_get_wtime();

    int iSize = vectMeasuresInputFIFO.size();
    if (iSize >= 3)
    {
        sTargetInfoInFrame infoinframe1, infoinframe2, infoinframe3;
        infoinframe1.stimeFrame = vectMeasuresInputFIFO[iSize - 3].stimeFrame;
        infoinframe1.ulFrameSeq = vectMeasuresInputFIFO[iSize - 3].ulFrameSeq;
        infoinframe1.blobMeasure = m_vectManualAddTop3blob[0];
        infoinframe1.bValid = true;

        infoinframe2.stimeFrame = vectMeasuresInputFIFO[iSize - 2].stimeFrame;
        infoinframe2.ulFrameSeq = vectMeasuresInputFIFO[iSize - 2].ulFrameSeq;
        infoinframe2.blobMeasure = m_vectManualAddTop3blob[1];
        infoinframe2.bValid = true;

        infoinframe3.stimeFrame = vectMeasuresInputFIFO[iSize - 1].stimeFrame;
        infoinframe3.ulFrameSeq = vectMeasuresInputFIFO[iSize - 1].ulFrameSeq;
        infoinframe3.blobMeasure = m_vectManualAddTop3blob[2];
        infoinframe3.bValid = true;

        sTargetInfo infoAssoc;
        infoAssoc.vectInfoInFrame.push_back(infoinframe1);
        infoAssoc.vectInfoInFrame.push_back(infoinframe2);
        infoAssoc.vectInfoInFrame.push_back(infoinframe3);
        infoAssoc.pairfPredSpdInFrame.first = ((infoinframe2.blobMeasure.pairfPos.first - infoinframe1.blobMeasure.pairfPos.first) + (infoinframe3.blobMeasure.pairfPos.first - infoinframe2.blobMeasure.pairfPos.first)) / 2.0;
        infoAssoc.pairfPredSpdInFrame.second = ((infoinframe2.blobMeasure.pairfPos.second - infoinframe1.blobMeasure.pairfPos.second) + (infoinframe3.blobMeasure.pairfPos.second - infoinframe2.blobMeasure.pairfPos.second)) / 2.0;
        infoAssoc.pairfPredPosInFrame.first = infoinframe3.blobMeasure.pairfPos.first + infoAssoc.pairfPredSpdInFrame.first;
        infoAssoc.pairfPredPosInFrame.second = infoinframe3.blobMeasure.pairfPos.second + infoAssoc.pairfPredSpdInFrame.second;
        float fPeriod = 1.0 / m_fFrameFreq;
        infoAssoc.pairfPredSpdAE.first = ((infoinframe2.blobMeasure.pairfPosAE.first - infoinframe1.blobMeasure.pairfPosAE.first) + (infoinframe3.blobMeasure.pairfPosAE.first - infoinframe2.blobMeasure.pairfPosAE.first)) / fPeriod / 2.0;
        infoAssoc.pairfPredSpdAE.second = ((infoinframe2.blobMeasure.pairfPosAE.second - infoinframe1.blobMeasure.pairfPosAE.second) + (infoinframe3.blobMeasure.pairfPosAE.second - infoinframe2.blobMeasure.pairfPosAE.second)) / fPeriod / 2.0;
        infoAssoc.pairfPredPosAE.first = infoinframe3.blobMeasure.pairfPosAE.first + infoAssoc.pairfPredSpdAE.first * fPeriod;
        infoAssoc.pairfPredPosAE.second = infoinframe3.blobMeasure.pairfPosAE.second + infoAssoc.pairfPredSpdAE.second * fPeriod;
        infoAssoc.fValid = 1.0;
        infoAssoc.bLiving = true;

        vectTargetInfo.push_back(infoAssoc);

        if(m_vectManualAddTop3blob.size() == 3)
            m_vectManualAddTop3blob.clear();
    }

    return 0;
}

int TrackAlgo::MANUAL_FindTargets(vector<sMeasuresInFrame> vectMeasuresInputFIFO, vector<sTargetInfo>& vectTargetInfo, float fRadius, float fThresh, sOpticParams opticparams, const sMeasureBlob& ManualBlob)
{
    int iFIFOSize = vectMeasuresInputFIFO.size();
    if(iFIFOSize <= 3)
    {
        sMeasureBlob sBlobCur = ManualBlob;
        m_vectManualAddTop3blob.push_back(sBlobCur);
    }
    if (iFIFOSize >= 3)
    {
        /// 三帧关联
        vector<sTargetInfo> vectTargetInfoTemp;
        if (MANUAL_Assoc3(vectMeasuresInputFIFO, vectTargetInfoTemp, fRadius, fThresh, opticparams) == 0)
        {
            vectTargetInfo = vectTargetInfoTemp;
            if (vectTargetInfo.size() > 0)
            {
                m_bTargetFind = true;
            }
        }
        else
        {
            return 2;
        }
    }
    else
    {
        return 1;
    }

    return 0;
}

int TrackAlgo::MANUAL_VerifyTarget(vector<sMeasuresInFrame> vectMeasuresInputFIFO, vector<sTargetInfo>& vectTargetInfo, sTargetInfo& targetInfo, float fThresh, sOpticParams opticparams, float fThreashLiving, int iNumFramesLiving, const sMeasureBlob& ManualBlob)
{
    if (vectMeasuresInputFIFO.size() >= 3 && vectTargetInfo.size() > 0)
    {
        bool bLivingTotal = false;

            for (int i = 0; i < vectTargetInfo.size(); i++)
            {
                if (vectTargetInfo[i].bLiving)
                {
                    sTargetInfoInFrame infoTrack;
                    {
                        infoTrack.stimeFrame = vectMeasuresInputFIFO.at(vectMeasuresInputFIFO.size() - 1).stimeFrame;
                        infoTrack.ulFrameSeq = vectMeasuresInputFIFO.at(vectMeasuresInputFIFO.size() - 1).ulFrameSeq;
                        infoTrack.blobMeasure = ManualBlob;
                        infoTrack.bValid = true;
                    }
                    vectTargetInfo[i].vectInfoInFrame.push_back(infoTrack);
                    int iSize = vectTargetInfo[i].vectInfoInFrame.size();
                    vectTargetInfo[i].pairfPredSpdInFrame.first = MedianFloat3((vectTargetInfo[i].vectInfoInFrame[iSize - 1].blobMeasure.pairfPos.first - vectTargetInfo[i].vectInfoInFrame[iSize - 2].blobMeasure.pairfPos.first),
                            (vectTargetInfo[i].vectInfoInFrame[iSize - 2].blobMeasure.pairfPos.first - vectTargetInfo[i].vectInfoInFrame[iSize - 3].blobMeasure.pairfPos.first),
                            (vectTargetInfo[i].vectInfoInFrame[iSize - 3].blobMeasure.pairfPos.first - vectTargetInfo[i].vectInfoInFrame[iSize - 4].blobMeasure.pairfPos.first));
                    vectTargetInfo[i].pairfPredSpdInFrame.second = MedianFloat3((vectTargetInfo[i].vectInfoInFrame[iSize - 1].blobMeasure.pairfPos.second - vectTargetInfo[i].vectInfoInFrame[iSize - 2].blobMeasure.pairfPos.second),
                            (vectTargetInfo[i].vectInfoInFrame[iSize - 2].blobMeasure.pairfPos.second - vectTargetInfo[i].vectInfoInFrame[iSize - 3].blobMeasure.pairfPos.second),
                            (vectTargetInfo[i].vectInfoInFrame[iSize - 3].blobMeasure.pairfPos.second - vectTargetInfo[i].vectInfoInFrame[iSize - 4].blobMeasure.pairfPos.second));
                    vectTargetInfo[i].pairfPredPosInFrame.first = vectTargetInfo[i].vectInfoInFrame[iSize - 1].blobMeasure.pairfPos.first + vectTargetInfo[i].pairfPredSpdInFrame.first;
                    vectTargetInfo[i].pairfPredPosInFrame.second = vectTargetInfo[i].vectInfoInFrame[iSize - 1].blobMeasure.pairfPos.second + vectTargetInfo[i].pairfPredSpdInFrame.second;
                    float fPeriod = 1.0 / m_fFrameFreq;
                    vectTargetInfo[i].pairfPredSpdAE.first = MedianFloat3((vectTargetInfo[i].vectInfoInFrame[iSize - 1].blobMeasure.pairfPosAE.first - vectTargetInfo[i].vectInfoInFrame[iSize - 2].blobMeasure.pairfPosAE.first),
                            (vectTargetInfo[i].vectInfoInFrame[iSize - 2].blobMeasure.pairfPosAE.first - vectTargetInfo[i].vectInfoInFrame[iSize - 3].blobMeasure.pairfPosAE.first),
                            (vectTargetInfo[i].vectInfoInFrame[iSize - 3].blobMeasure.pairfPosAE.first - vectTargetInfo[i].vectInfoInFrame[iSize - 4].blobMeasure.pairfPosAE.first)) / fPeriod;
                    vectTargetInfo[i].pairfPredSpdAE.second = MedianFloat3((vectTargetInfo[i].vectInfoInFrame[iSize - 1].blobMeasure.pairfPosAE.second - vectTargetInfo[i].vectInfoInFrame[iSize - 2].blobMeasure.pairfPosAE.second),
                            (vectTargetInfo[i].vectInfoInFrame[iSize - 2].blobMeasure.pairfPosAE.second - vectTargetInfo[i].vectInfoInFrame[iSize - 3].blobMeasure.pairfPosAE.second),
                            (vectTargetInfo[i].vectInfoInFrame[iSize - 3].blobMeasure.pairfPosAE.second - vectTargetInfo[i].vectInfoInFrame[iSize - 4].blobMeasure.pairfPosAE.second)) / fPeriod;
                    vectTargetInfo[i].pairfPredPosAE.first = vectTargetInfo[i].vectInfoInFrame[iSize - 1].blobMeasure.pairfPosAE.first + vectTargetInfo[i].pairfPredSpdAE.first * fPeriod;
                    vectTargetInfo[i].pairfPredPosAE.second = vectTargetInfo[i].vectInfoInFrame[iSize - 1].blobMeasure.pairfPosAE.second + vectTargetInfo[i].pairfPredSpdAE.second * fPeriod;
                    vectTargetInfo[i].fValid = ((iSize - 1) * vectTargetInfo[i].fValid + (vectTargetInfo[i].vectInfoInFrame[iSize - 1].bValid ? 1 : 0)) / iSize;
                    vectTargetInfo[i].bLiving = true;
                }
            }
            m_bTargetVerify = true;
            targetInfo = vectTargetInfo[0];
    }
    else
    {
        return 1;
    }

    return 0;
}

int TrackAlgo::MANUAL_TrackTarget(vector<sMeasuresInFrame> vectMeasuresInputFIFO, sTargetInfo& targetInfo, float fThresh, sOpticParams opticparams, float fThreashLiving, int iNumFramesLiving, bool bAutoDecide, const sMeasureBlob& ManualBlob)
{
    if (vectMeasuresInputFIFO.size() >= 3)
    {
        if (targetInfo.bLiving)
        {
            sTargetInfoInFrame infoTrack;
            {
                infoTrack.stimeFrame = vectMeasuresInputFIFO.at(vectMeasuresInputFIFO.size() - 1).stimeFrame;
                infoTrack.ulFrameSeq = vectMeasuresInputFIFO.at(vectMeasuresInputFIFO.size() - 1).ulFrameSeq;
                infoTrack.blobMeasure = ManualBlob;
                infoTrack.bValid = true;
            }
            targetInfo.vectInfoInFrame.push_back(infoTrack);
            int iSize = targetInfo.vectInfoInFrame.size();
            targetInfo.pairfPredSpdInFrame.first = MedianFloat3((targetInfo.vectInfoInFrame[iSize - 1].blobMeasure.pairfPos.first - targetInfo.vectInfoInFrame[iSize - 2].blobMeasure.pairfPos.first),
                    (targetInfo.vectInfoInFrame[iSize - 2].blobMeasure.pairfPos.first - targetInfo.vectInfoInFrame[iSize - 3].blobMeasure.pairfPos.first),
                    (targetInfo.vectInfoInFrame[iSize - 3].blobMeasure.pairfPos.first - targetInfo.vectInfoInFrame[iSize - 4].blobMeasure.pairfPos.first));
            targetInfo.pairfPredSpdInFrame.second = MedianFloat3((targetInfo.vectInfoInFrame[iSize - 1].blobMeasure.pairfPos.second - targetInfo.vectInfoInFrame[iSize - 2].blobMeasure.pairfPos.second),
                    (targetInfo.vectInfoInFrame[iSize - 2].blobMeasure.pairfPos.second - targetInfo.vectInfoInFrame[iSize - 3].blobMeasure.pairfPos.second),
                    (targetInfo.vectInfoInFrame[iSize - 3].blobMeasure.pairfPos.second - targetInfo.vectInfoInFrame[iSize - 4].blobMeasure.pairfPos.second));
            targetInfo.pairfPredPosInFrame.first = targetInfo.vectInfoInFrame[iSize - 1].blobMeasure.pairfPos.first + targetInfo.pairfPredSpdInFrame.first;
            targetInfo.pairfPredPosInFrame.second = targetInfo.vectInfoInFrame[iSize - 1].blobMeasure.pairfPos.second + targetInfo.pairfPredSpdInFrame.second;
            float fPeriod = 1.0 / m_fFrameFreq;
            targetInfo.pairfPredSpdAE.first = MedianFloat3((targetInfo.vectInfoInFrame[iSize - 1].blobMeasure.pairfPosAE.first - targetInfo.vectInfoInFrame[iSize - 2].blobMeasure.pairfPosAE.first),
                    (targetInfo.vectInfoInFrame[iSize - 2].blobMeasure.pairfPosAE.first - targetInfo.vectInfoInFrame[iSize - 3].blobMeasure.pairfPosAE.first),
                    (targetInfo.vectInfoInFrame[iSize - 3].blobMeasure.pairfPosAE.first - targetInfo.vectInfoInFrame[iSize - 4].blobMeasure.pairfPosAE.first)) / fPeriod;
            targetInfo.pairfPredSpdAE.second = MedianFloat3((targetInfo.vectInfoInFrame[iSize - 1].blobMeasure.pairfPosAE.second - targetInfo.vectInfoInFrame[iSize - 2].blobMeasure.pairfPosAE.second),
                    (targetInfo.vectInfoInFrame[iSize - 2].blobMeasure.pairfPosAE.second - targetInfo.vectInfoInFrame[iSize - 3].blobMeasure.pairfPosAE.second),
                    (targetInfo.vectInfoInFrame[iSize - 3].blobMeasure.pairfPosAE.second - targetInfo.vectInfoInFrame[iSize - 4].blobMeasure.pairfPosAE.second)) / fPeriod;
            targetInfo.pairfPredPosAE.first = targetInfo.vectInfoInFrame[iSize - 1].blobMeasure.pairfPosAE.first + targetInfo.pairfPredSpdAE.first * fPeriod;
            targetInfo.pairfPredPosAE.second = targetInfo.vectInfoInFrame[iSize - 1].blobMeasure.pairfPosAE.second + targetInfo.pairfPredSpdAE.second * fPeriod;
            targetInfo.fValid = ((iSize - 1) * targetInfo.fValid + (targetInfo.vectInfoInFrame[iSize - 1].bValid ? 1 : 0)) / iSize;
//            if (bAutoDecide)
//            {
//                targetInfo.bLiving = !(targetInfo.vectInfoInFrame.size() >= iNumFramesLiving && targetInfo.fValid < fThreashLiving && !targetInfo.vectInfoInFrame[targetInfo.vectInfoInFrame.size() - 1].bValid);
//            }
            targetInfo.bLiving = true;
        }
        else
        {
            InitTargetInfo();
            return 2;
        }
    }
    else
    {
        return 1;
    }

    return 0;
}

int TrackAlgo::GetVectTargetInfo(vector<sTargetInfo>& vectTargetInfo)
{
	vectTargetInfo = m_vectTargetInfo;

	return 0;
}

int TrackAlgo::GetTargetInfo(sTargetInfo& targetInfo)
{
	targetInfo = m_targetInfo;

	return 0;
}

int TrackAlgo::InitTargetInfo(void)
{
    if (!m_vectTargetInfoFind.empty())
    {
        m_vectTargetInfoFind.clear();
    }

	if (!m_vectTargetInfo.empty())
	{
		m_vectTargetInfo.clear();
	}

    m_targetInfo.bLiving = false;
    m_targetInfo.fValid = 0;
    m_targetInfo.qstrTargetID = "";
    if (m_targetInfo.vectInfoInFrame.size() > 0)
        m_targetInfo.vectInfoInFrame.clear();
    if (m_targetInfo.vectResPacket.size() > 0)
        m_targetInfo.vectResPacket.clear();
    if (m_targetInfo.vectulFrameSeqTWDW.size() > 0)
        m_targetInfo.vectulFrameSeqTWDW.clear();
    if (m_targetInfo.vectulFrameSeqStore.size() > 0)
        m_targetInfo.vectulFrameSeqStore.clear();    
    m_targetInfo.pairfPredPosAE = pair<float,float>(0,0);
    m_targetInfo.pairfPredSpdAE = pair<float,float>(0,0);
    m_targetInfo.pairfPredPosInFrame = pair<float,float>(0,0);
    m_targetInfo.pairfPredSpdInFrame = pair<float,float>(0,0);

	m_bTargetFind = false;
    m_bTargetVerify = false;

	return 0;
}

int TrackAlgo::InitFIFO_Target(void)
{
	m_vectFIFO_Target.clear();

	return 0;
}

int TrackAlgo::InitFIFO_Star(void)
{
	m_vectFIFO_Star.clear();

    return 0;
}

int TrackAlgo::InitTracker(int iTrackMode, sTrackingSettings settings, int iTargetID)
{
	m_settings = settings;
	InitFIFO_Target();
	InitFIFO_Star();
	InitTargetInfo();
	m_ulFrameSeq = 0;
	m_iMode = iTrackMode;
    m_iTargetID = iTargetID;

	return 0;
}

int TrackAlgo::CalcStarSpd(sMeasuresInFrame measures, pair<float, float>& pairfSpd, pair<float, float>& pairfSpd_AE, int& iNumMost)
{
	m_vectFIFO_Star.push_back(measures);
    if (m_iMode == TRACK_GEO)
	{
        int iNumBigBlob = 0;
        for (int i = 0; i < measures.vectStarMeasures.size(); i++)
        {
            if (measures.vectStarMeasures[i].fArea > 200)
                iNumBigBlob++;
        }
        float fRatioBigBlob = (float)iNumBigBlob / measures.vectStarMeasures.size();
        float fRadius = m_iMode == fRatioBigBlob > 0.5 ? 200 : m_settings.fRadius;    // MEO large radius
        m_pGParam->m_SImageProcessorData.fRadiusS = fRadius;
        if (CalcStarSpd(m_vectFIFO_Star, m_settings.fRatioFOV, fRadius, m_settings.fThreashGazeMode, m_settings.opticparams, pairfSpd, pairfSpd_AE, iNumMost) == 0)
        {
            if (iNumMost >= 5)
            {
                m_pairfStarSpd = pairfSpd;
                m_pairfStarSpd_AE = pairfSpd_AE;
            }
            else
            {
                m_pairfStarSpd = pair<float,float>(-512.0,346.0);
                m_pairfStarSpd = pair<float,float>(-2.345,5.367);
            }
        }
        else
        {
            if (m_vectFIFO_Star.size() > 3)
            {
                vector<sMeasuresInFrame>::iterator k = m_vectFIFO_Star.begin();
                m_vectFIFO_Star.erase(k);	// 删除第一个元素
            }

            return 2;
        }
	}
	else
	{
		if (m_vectFIFO_Star.size() > 3)
		{
			vector<sMeasuresInFrame>::iterator k = m_vectFIFO_Star.begin();
			m_vectFIFO_Star.erase(k);	// 删除第一个元素
		}

		return 1;
	}
	if (m_vectFIFO_Star.size() > 3)
	{
		vector<sMeasuresInFrame>::iterator k = m_vectFIFO_Star.begin();
		m_vectFIFO_Star.erase(k);	// 删除第一个元素
	}

    return 0;
}

int TrackAlgo::KillTarget()
{
	if (m_bTargetFind && m_bTargetVerify)
	{
		m_targetInfo.bLiving = false;
	}
	else
	{
		return 1;
	}

	return 0;
}

void TrackAlgo::CalcDiffTime(sTime beginTime, sTime endTime, double& dDiffSec)
{
    if (beginTime.iDay == endTime.iDay)
    {
        double dTime1 = beginTime.iHour * 60 * 60 + beginTime.iMinute * 60 + beginTime.iSecond + beginTime.iMillisecond * 0.001 + beginTime.iMicrosecond * 0.000001;
        double dTime2 = endTime.iHour * 60 * 60 + endTime.iMinute * 60 + endTime.iSecond + endTime.iMillisecond * 0.001 + endTime.iMicrosecond * 0.000001;
        dDiffSec = dTime2 - dTime1;
    }
    else
    {
        double dTime1 = beginTime.iHour * 60 * 60 + beginTime.iMinute * 60 + beginTime.iSecond + beginTime.iMillisecond * 0.001 + beginTime.iMicrosecond * 0.000001;
        double dTime2 = 86400 + endTime.iHour * 60 * 60 + endTime.iMinute * 60 + endTime.iSecond + endTime.iMillisecond * 0.001 + endTime.iMicrosecond * 0.000001;
        dDiffSec = dTime2 - dTime1;
    }
}

float TrackAlgo::MedianFloat3(float a, float b, float c)
{
    float max = a > b ? a : b;
    max = max > c ? max : c;
    float min = a < b ? a : b;
    min = min < c ? min : c;
    float median = a + b + c - max - min;

    return median;
}

float TrackAlgo::MedianFloat5(float a, float b, float c, float d, float e)
{
    float fData[5];
    fData[0] = a;
    fData[1] = b;
    fData[2] = c;
    fData[3] = d;
    fData[4] = e;
    float tmp = fData[0];
    for (int i = 0; i < 4; i++)
    {
        for (int j = 0; j < 4-i; j++)
        {
            if (fData[j] > fData[j+1])
            {
                tmp = fData[j];
                fData[j] = fData[j+1];
                fData[j+1] = tmp;
            }
        }
    }
    return fData[2];
}

void TrackAlgo::BJ2UTC(int iBJYear, int iBJMonth, int iBJDay, int iBJHour, int &iUTCYear, int &iUTCMonth, int &iUTCDay, int &iUTCHour)
{
    iUTCYear = iBJYear;
    iUTCMonth = iBJMonth;
    iUTCDay = iBJDay;
    iUTCHour = iBJHour - 8;
    if (iUTCHour < 0)
    {
        iUTCHour += 24;
        iUTCDay = iBJDay - 1;
        if (iUTCDay < 1)
        {
            switch (iBJMonth)
            {
            case 1:
                iUTCYear = iBJYear - 1;
                iUTCMonth = 12;
                iUTCDay = 31;
            break;
            case 2:
                iUTCYear = iBJYear;
                iUTCMonth = 1;
                iUTCDay = 31;
            break;
            case 3:
                iUTCYear = iBJYear;
                iUTCMonth = 2;
                iUTCDay = iBJYear % 4 == 0 ? 29 : 28;
            break;
            case 4:
                iUTCYear = iBJYear;
                iUTCMonth = 3;
                iUTCDay = 31;
            break;
            case 5:
                iUTCYear = iBJYear;
                iUTCMonth = 4;
                iUTCDay = 30;
            break;
            case 6:
                iUTCYear = iBJYear;
                iUTCMonth = 5;
                iUTCDay = 31;
            break;
            case 7:
                iUTCYear = iBJYear;
                iUTCMonth = 6;
                iUTCDay = 30;
            break;
            case 8:
                iUTCYear = iBJYear;
                iUTCMonth = 7;
                iUTCDay = 31;
            break;
            case 9:
                iUTCYear = iBJYear;
                iUTCMonth = 8;
                iUTCDay = 31;
            break;
            case 10:
                iUTCYear = iBJYear;
                iUTCMonth = 9;
                iUTCDay = 30;
            break;
            case 11:
                iUTCYear = iBJYear;
                iUTCMonth = 10;
                iUTCDay = 31;
            break;
            case 12:
                iUTCYear = iBJYear;
                iUTCMonth = 11;
                iUTCDay = 30;
            break;
            default:
            break;
            }
        }
    }
}

void TrackAlgo::AangleToEquator(double A, double E, double Longm, double phim, int y, int m, int d, int hour, int min, double sec, double p, double T, bool nflag, double *Ra, double *De, double *Rm, double *Dm, bool zflag)
{
    double dbAngleA = A;
    double dbAngleE = E;
    double dbAngleT;
    double dbAngleD; // 时角赤纬
    double w1;
    w1 = 60.2 / tan(dbAngleE) * AStR; // 大气折射，观测高度减掉折射量
    w1 = w1 * (273 * p / (1013 * T));
    if (nflag)
        dbAngleE = dbAngleE - w1; // 测量结果先修正大气折射再转换
    ////////////////////////////////////////////////////////
    double x1 = cos(dbAngleA) * cos(dbAngleE) * cos(phim) + sin(dbAngleE) * sin(phim);
    double y1 = -sin(dbAngleA) * cos(dbAngleE);
    double z1 = -cos(dbAngleA) * cos(dbAngleE) * sin(phim) + sin(dbAngleE) * cos(phim);
    dbAngleD = asin(x1);
    dbAngleT = atan2(y1, z1);
    //////////////////////////////
    double S0; // 格林尼治真恒星时
    double a0; // 黄赤交角
    double t0, tt, t;
    double b, a;
    double LongNut, ObliquityNut;
    double dbRm;
    double dbDm; // 赤经赤纬
    ///////////////////////////////////参数初始化
    t0 = JDE(y, m, d, hour, min, sec);
    tt = t0 * 36525.0 - floor(t0 * 36525.0); // 天的小数
    ObliquityNut = Nut2(t0);
    LongNut = Nut1(t0);
    //////////////真恒星时计算
    a0 = (23 * 3600 + 26 * 60 + 21.448 - 46.815 * t0 - 0.00059 * t0 * t0 + 0.001813 * t0 * t0 * t0) * AStR + ObliquityNut; // 单位为弧度
    S0 = (280.46061837 + 360.98564736629 * t0 * 36525 + 0.000387933 * t0 * t0 - t0 * t0 * t0 / 38710000) * DtR;            // 任意时刻格林尼治平恒星时
    S0 = S0 + LongNut * cos(a0);                                                                                           // 格林尼治真恒星时=平恒星时+赤经章动

    // t=S0+Longm-Rm;// 弧度//目标的瞬时时角=格林尼治真恒星时+当地经度-视赤经
    dbRm = S0 + Longm - dbAngleT;
    if (dbRm < 0)
        dbRm = dbRm + 2 * pi; // 赤经的测量值
    dbRm = fmod(dbRm, 2 * pi);
    *Ra = dbAngleT;
    *De = dbAngleD;

    *Rm = dbRm;
    *Dm = dbAngleD;
}

int TrackAlgo::getExponent(double num)
{
    if (num == 0)
    {
        return 0; // 处理特殊情况：对于零，指数部分为 0
    }
    else
    {
        double absNum = fabs(num);       // 获取数值的绝对值
        double exponent = log10(absNum); // 获取对数值
        return static_cast<int>(floor(exponent)); // 返回指数部分（取整）
    }
}

double TrackAlgo::JDE(int Y, int M, int D, int hour, int min, double sec)
{
    int f, g;
    double Mid1, Mid2, A, J, JDE;
    if (M >= 3)
    {
        f = Y;
        g = M;
    }
    if (M < 3)
    {
        f = Y - 1;
        g = M + 12;
    }
    Mid1 = floor(365.25 * f);
    Mid2 = floor(30.6001 * (g + 1));
    A = 2 - floor(f / 100) + floor(f / 400);
    J = Mid1 + Mid2 + D + A + 1720994.5;
    JDE = J + hour / 24.0 + min / 1440.0 + sec / 86400.0;

    JDE = (JDE - 2451545.0) / 36525.0;
    return JDE;
}

double TrackAlgo::Nut1(double t0)
{
    double LongNut;
    double L0, L1, f, D, Q;
    double s;
    L0 = 2.355548393544 + (8328.6914228839 + (0.000151795164 + 0.00000031028076 * t0) * t0) * t0; // 此五项为弧度
    L1 = 6.240035939326 + (628.30195602418 - (0.00000279737494 + 0.000000058177642 * t0) * t0) * t0;
    f = 3.255803867944 + (16866.9323166370 - (0.0001285434994 - 0.00000010665901 * t0) * t0) * t0;
    D = 5.198469513189 + (7771.377146171 - (0.0000334085108 - 0.0000000921146 * t0) * t0) * t0;
    Q = 2.182438624361 - (33.757045933754 - (0.0000361428599 + 0.000000038785094 * t0) * t0) * t0;

    i0 = cos(L0);
    i1 = sin(L0);
    i2 = cos(L1);
    i3 = sin(L1);
    i4 = cos(f);
    i5 = sin(f);
    i6 = cos(D);
    i7 = sin(D);
    i8 = cos(Q);
    i9 = sin(Q);
    j00 = 2 * i0 * i0 - 1;
    j11 = 2 * i0 * i1;
    j2 = 2 * i2 * i2 - 1;
    j3 = 2 * i2 * i3;
    j4 = 2 * i6 * i6 - 1;
    j5 = 2 * i6 * i7;
    j6 = 2 * i8 * i8 - 1;
    j7 = 2 * i8 * i9;
    j8 = i4 * j6 - i5 * j7;
    j9 = i5 * j6 + i4 * j7;
    k0 = i4 * i8 - i5 * i9;
    k1 = i5 * i8 + i4 * i9;
    k2 = i0 * j4 + i1 * j5;
    k3 = i1 * j4 - i0 * j5;
    k4 = k0 * j4 + k1 * j5;
    k5 = k1 * j4 - k0 * j5;
    k6 = j8 * j4 - j9 * j5;
    k7 = j9 * j4 + j8 * j5;
    k8 = j8 * j4 + j9 * j5;
    k9 = j9 * j4 - j8 * j5;
    l0 = j8 * i0 - j9 * i1;
    l1 = j9 * i0 + j8 * i1;
    l2 = j00 * j4 + j11 * j5;
    l3 = j11 * j4 - j00 * j5;
    l4 = j00 * i4 + j11 * i5;
    l5 = j11 * i4 - j00 * i5;
    l6 = i8 * j4 - i9 * j5;
    l7 = i9 * j4 + i8 * j5;
    l8 = k8 * j00 - k9 * j11;
    l9 = k9 * j00 + k8 * j11;
    m0 = k6 * k2 + k7 * k3;
    m1 = k7 * k2 - k6 * k3;
    m2 = i0 * i6 + i1 * i7;
    m3 = i1 * i6 - i0 * i7;
    m4 = i4 * j4 + i5 * j5;
    m5 = i5 * j4 - i4 * j5;
    m6 = i2 * k2 - i3 * k3;
    m7 = i3 * k2 + i2 * k3;
    m8 = i0 * j4 - i1 * j5;
    m9 = i1 * j4 + i0 * j5;

    //
    s = t0 / 10;
    // s=t0;
    // 计算黄经章动，单位为0.0001角秒

    LongNut =
        k0 * m9 + (4 * j8 - 2 * i8) * m7 + (i0 - 22) * m5 - (4 + 3 * i2 + i8) * m3 - (2 * i0 + 2) * m1 +
        (30 * i0 + 6) * l9 - (6 * i4 + i0) * l7 + (i8 - l0 + 11) * l5 + (10 * i8 + i2) * l3 +
        (l4 - 3 * j00 - i2) * l1 + ((s - 16) * j2 + i4 + (7 * s - 300) * i2 - 16 * s - 13187) * k9 - (3 * i2 + 67 * i0 + 38) * k7 + (j00 - 2 * j2 - i2 + s + 129) * k5 +
        (16 * k0 - 2 * (k2 + i4) - j4 - 29 * i8 - 6 * i2) * k3 +
        (41 * j00 - 3 * m8 - 4 * k2 - 30 * i0 - 4 * s - 386) * k1 + (i6 - 34 * j00 - 178 * i0 - 2 * (m6 + s) - 2274) * j9 +
        (i2 - i0 + 2 * s + 2062) * j7 + (k2 - 47 * j00 - i8 + 3 * i2 + 164 * i0 + 63) * j5 + (k8 * (s - 16) + 2 * k4 - s + 17) * j3 + (k4 - 3 * l0 - 2 * (k6 - i0) - 51 * k0 - 28 * j8 + 49 * j4 + 4 * i8 + 29) * j11 +
        (m2 + l4 - 2 * l2 + 3 * k2 - 11 * j4 - i4 - 27 * i2 + 5 * i0 - 1742 * s - 171996) * i9 +
        (3 * j8 + i2 - 4) * i7 + (k8 - 8 * l6 + i8 - i0 + 26) * i5 + (l2 - 2 * (m4 - k0) + 3 * (m2 + k6 - i8) + 5 * (l0 - j4) + (17 * s - 734) * k8 + 9 * k4 - 8 * (k2 + i0) + 14 * j8 + j6 + i6 - 34 * s + 1426) * i3 +
        (2 * (j00 + i2) - m4 - 28 * l8 - l6 + 51 * k6 - 72 * k0 - 424 * j8 - 3 * j6 - 152 * j4 +
         (2 * s + 121) * i8 + 7 * i4 + s + 712) *
            i1 -
        29 * (l9 * i0 - l8 * i1) + 29 * (k9 * i0 + k8 * i1);

    LongNut = LongNut * 0.0001 * AStR; // 结果为弧度

    return LongNut;
}

double TrackAlgo::Nut2(double t0)
{
    double ObliquityNut;
    double s;
    s = t0 / 10;
    // s=t0;
    ObliquityNut = 8 * k3 * k1 + 2 * ((m8 + k2 + 100) * k0 + m7 * j9) + (k5 - 7 * k9) * j3 + (7 * k8 + k4) * j2 + (k5 - l1 - k7 - 27 * k1 - 12 * j9) * j11 + (l0 - k6 - k4 - 21 * k0 + 14 * j8 - 1) * j00 + (5 * l3 - 15 * k3 + 2 * j11) * i9 + (l2 - k2 + 6 * j4 + 89 * s + 92025) * i8 + j9 * i7 - j8 * i6 + 3 * (l6 * i4 - l7 * i5 - l8) + (2 * l1 + (9 * s - 319) * k9 + k7 + 5 * k5 + 6 * j9 - 3 * i9) * i3 + ((129 - 3 * s) * k8 + k6 + k4 + 15 * i8 - s + 54) * i2 + (m1 - 12 * l9 + 23 * k7 - 37 * k1 + (s - 182) * j9 - 2 * j7 + 65 * i9) * i1 + (m0 - 12 * l8 + 29 * k6 + 17 * k0 - (s - 76) * j8 - i8 - 7) * i0 + m0 + l2 - (31 * s - 5736) * k8 + 16 * k6 - 70 * k4 - k2 - (5 * s - 977) * j8 + (5 * s - 895) * j6 - 2 * j4 - j00 - i4 + 12 * (l8 * i0 + l9 * i1) - 12 * (k8 * i0 - k9 * i1);

    ObliquityNut = ObliquityNut * 0.0001 * AStR; // 结果为弧度

    return ObliquityNut;
}


/// 备注
// 1. 在检测过程中,在fThresh范围内的点与航迹全部被压缩为1条,亦即只要存在1个在fThresh范围内的相邻测量点,则航迹将被压缩为1条;
//    对于平行或相交航迹,被压缩掉一条航迹;虽然这不符合实际情况,但能够有效避免航迹混叠;另外,航迹的实际情况存在稀疏化、平行化特征,不太容易出现此种情况;即便存在伴星,亦将超出fThresh的范围;
// 2. 在跟踪过程中,会出现测量点竞争情况,这需要加入卡尔曼滤波,从而降低错误选择测量点对跟踪性能的影响;
// 3. 在重新检测过程中,只要跟之前航迹存在相同测量点的航迹将被压缩,相邻测量点对于旧航迹没有影响,对于新航迹,则如1中的相交航迹检测情况,会出现新航迹被压缩情况;  
// 4. 关于vector的capacity问题,当vector容器大小超过capacity时,会出现迭代器失效问题,但本程序没有边迭代循环边改变vector的用法,因此无此问题
// 5. 中高轨搜索与跟踪用内触发,低轨跟踪用外触发,因此均为等差数列,不用考虑时间误差问题
// 6. 跟踪过程的openmp循环放到里边,效率更高,特别是对低轨目标跟踪有极大优势
// 7. 即便是低轨跟踪,也不需要卡尔曼滤波,因为速度变化极为平缓,三帧速度平滑已经可以预测的非常准确
// 8. 帧内速度单位均为pixel/frame,AE速度单位均为°/s
// 9. 低轨跟踪必须用AE进行目标航迹关联与跟踪,因为考虑到轨道预报误差及伺服对脱靶量或速度的修正,目标在图像空间的运动轨迹可能无法建模描述.同时,低轨跟踪要严格限制每帧的测量值数量,否则关联与跟踪运算时间过长
