 /***********************************************************************
 功能：一个高精度计时器，用来计算程序的帧数/间隔时间
 作者：Ray1024
 网址：http://www.cnblogs.com/Ray1024/
 ***********************************************************************/

#pragma once

#include "Utils.h"

// 计时器
class D2D1Timer
{
public:
	// 构造函数
	D2D1Timer()
	{
		__int64 countsPerSec;
		QueryPerformanceFrequency((LARGE_INTEGER*)&countsPerSec);
		m_secondsPerCount = 1.0 / (double)countsPerSec;
	}
	// 总时间，单位为秒
	float TotalTime() const
	{
		if (m_stopped)
			return (float)(((m_stopTime - m_pausedTime) - m_baseTime)*m_secondsPerCount);
		else
			return (float)(((m_currTime - m_pausedTime) - m_baseTime)*m_secondsPerCount);
	}
	// 间隔时间，单位为秒
	float DeltaTime() const { return (float)m_deltaTime; }

	// 消息循环前调用
	void Reset()
	{
		__int64 currTime;
		QueryPerformanceCounter((LARGE_INTEGER*)&currTime);

		m_baseTime = currTime;
		m_prevTime = currTime;
		m_stopTime = 0;
		m_stopped = false;
	}
	// 取消暂停时调用
	void Start()
	{
		__int64 startTime;
		QueryPerformanceCounter((LARGE_INTEGER*)&startTime);

		if (m_stopped)
		{
			// 则累加暂停时间
			m_pausedTime += (startTime - m_stopTime);
			// 因为我们重新开始计时，因此m_prevTime的值就不正确了，
			// 要将它重置为当前时间 
			m_prevTime = startTime;
			// 取消暂停状态
			m_stopTime = 0;
			m_stopped = false;
		}
	}
	// 暂停时调用
	void Stop()
	{
		// 如果正处在暂停状态，则略过下面的操作
		if (!m_stopped)
		{
			__int64 currTime;
			QueryPerformanceCounter((LARGE_INTEGER*)&currTime);

			// 记录暂停的时间，并设置表示暂停状态的标志
			m_stopTime = currTime;
			m_stopped = true;
		}
	}
	// 每帧调用
	void Tick()
	{
		if (m_stopped)
		{
			m_deltaTime = 0.0;
			return;
		}

		__int64 currTime;
		QueryPerformanceCounter((LARGE_INTEGER*)&currTime);
		m_currTime = currTime;

		// 当前帧和上一帧之间的时间差
		m_deltaTime = (m_currTime - m_prevTime)*m_secondsPerCount;

		// 为计算下一帧做准备
		m_prevTime = m_currTime;

		// 确保不为负值。DXSDK中的CDXUTTimer提到：如果处理器进入了节电模式
		// 或切换到另一个处理器，m_deltaTime会变为负值。
		if (m_deltaTime < 0.0)
		{
			m_deltaTime = 0.0;
		}
	}

private:
	double						m_secondsPerCount = 0.0;
	double						m_deltaTime = -1.0;

	__int64						m_baseTime = 0;
	__int64						m_pausedTime = 0;
	__int64						m_stopTime = 0;
	__int64						m_prevTime = 0;
	__int64						m_currTime = 0;

	bool						m_stopped = false;
};
