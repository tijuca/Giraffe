/*
 * Copyright 2005 - 2012  Zarafa B.V.
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License, version 3, 
 * as published by the Free Software Foundation with the following additional 
 * term according to sec. 7:
 *  
 * According to sec. 7 of the GNU Affero General Public License, version
 * 3, the terms of the AGPL are supplemented with the following terms:
 * 
 * "Zarafa" is a registered trademark of Zarafa B.V. The licensing of
 * the Program under the AGPL does not imply a trademark license.
 * Therefore any rights, title and interest in our trademarks remain
 * entirely with us.
 * 
 * However, if you propagate an unmodified version of the Program you are
 * allowed to use the term "Zarafa" to indicate that you distribute the
 * Program. Furthermore you may use our trademarks where it is necessary
 * to indicate the intended purpose of a product or service provided you
 * use it in accordance with honest practices in industrial or commercial
 * matters.  If you want to propagate modified versions of the Program
 * under the name "Zarafa" or "Zarafa Server", you may only do so if you
 * have a written permission by Zarafa B.V. (to acquire a permission
 * please contact Zarafa at trademark@zarafa.com).
 * 
 * The interactive user interface of the software displays an attribution
 * notice containing the term "Zarafa" and/or the logo of Zarafa.
 * Interactive user interfaces of unmodified and modified versions must
 * display Appropriate Legal Notices according to sec. 5 of the GNU
 * Affero General Public License, version 3, when you propagate
 * unmodified or modified versions of the Program. In accordance with
 * sec. 7 b) of the GNU Affero General Public License, version 3, these
 * Appropriate Legal Notices must retain the logo of Zarafa or display
 * the words "Initial Development by Zarafa" if the display of the logo
 * is not reasonably feasible for technical reasons. The use of the logo
 * of Zarafa in Legal Notices is allowed for unmodified and modified
 * versions of the software.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Affero General Public License for more details.
 *  
 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 * 
 */

#ifndef ECThreadPool_INCLUDED
#define ECThreadPool_INCLUDED

#include <pthread.h>
#include <set>
#include <list>


class ECTask;

/**
 * This class represents a thread pool with a fixed amount of worker threads.
 * The amount of workers can be modified at run time, but is not automatically
 * adjusted based on the task queue length or age.
 */
class ECThreadPool
{
private:	// types
	struct STaskInfo {
		ECTask			*lpTask;
		bool			bDelete;
		struct timeval	tvQueueTime;
	};

	typedef std::set<pthread_t> ThreadSet;
	typedef std::list<STaskInfo> TaskList;
	
public:
	ECThreadPool(unsigned ulThreadCount);
	~ECThreadPool();
	
	virtual bool dispatch(ECTask *lpTask, bool bTakeOwnership = false);
	unsigned threadCount() const;
	void setThreadCount(unsigned ulThreadCount, bool bWait = false);
	
	struct timeval queueAge() const;
	
private:	// methods
	virtual bool getNextTask(STaskInfo *lpsTaskInfo);
	void joinTerminated();
	
private:	// static methods
	static void *threadFunc(void *lpVoid);
	static bool isCurrentThread(const pthread_t &hThread);
	
private:	// members
	ThreadSet	m_setThreads;
	ThreadSet	m_setTerminated;
	TaskList	m_listTasks;
	
	mutable pthread_mutex_t	m_hMutex;
	pthread_cond_t			m_hCondition;
	pthread_cond_t			m_hCondTerminated;

private:
	ECThreadPool(const ECThreadPool &);
	ECThreadPool& operator=(const ECThreadPool &);
	
	unsigned	m_ulTermReq;
};

/**
 * Get the number of worker threads.
 * @retval The number of available worker threads.
 */
inline unsigned ECThreadPool::threadCount() const {
	return m_setThreads.size() - m_ulTermReq;
}




/**
 * This class represents a threadpool with a dynamic amount of worker threads.
 * The amount of threads is based on the task queue length and age.
 *
 * Note: This is class is not implemented
 */
class ECDynamicThreadPool : public ECThreadPool
{
public:
	ECDynamicThreadPool(unsigned ulMinThreads, unsigned ulMaxThreads, unsigned ulMaxQueueAge = 500, unsigned ulThreadTimeout = 5000);
	~ECDynamicThreadPool();
	
	virtual bool dispatch(ECTask *lpTask, bool bTakeOwnership = false);
	
	unsigned minThreadCount() const;
	void setMinThreadCount(unsigned ulMinThreads);
	
	unsigned maxThreadCount() const;
	void setMaxThreadCount(unsigned ulMaxThreads);
	
	unsigned maxQueueAge() const;
	void setMaxQueueAge(unsigned ulMaxQueueAge);

	unsigned maxQueueLength() const;
	void setMaxQueueLength() const;
	
	unsigned threadTimeout() const;
	void setThreadTimeout(unsigned ulThreadTimeout);
};



/**
 * This class represents a task that can be dispatched on an ECThreadPool or
 * derived object.
 * Once dispatched, the objects run method will be executed once the threadpool
 * has a free worker and all previously queued tasks have been processed. There's
 * no way of knowing when the task is done.
 */
class ECTask
{
public:
	virtual ~ECTask();
	virtual void execute();
	
	bool dispatchOn(ECThreadPool *lpThreadPool, bool bTransferOwnership = false);
	
protected:
	virtual void run() = 0;
	ECTask();
	
private:
	// Make the object non-copyable
	ECTask(const ECTask &);
	ECTask &operator=(const ECTask &);
};

/**
 * Dispatch a task object on a particular threadpool.
 *
 * @param[in]	lpThreadPool		The threadpool on which to dispatch the task.
 * @param[in]	bTransferOwnership	Boolean parameter specifying wether the threadpool
 *                                  should take ownership of the task object, and thus
 *                                  is responsible for deleting the object when done.
 * @retval true if the task was successfully queued, false otherwise.
 */
inline bool ECTask::dispatchOn(ECThreadPool *lpThreadPool, bool bTransferOwnership) {
	return lpThreadPool ? lpThreadPool->dispatch(this, bTransferOwnership) : false;
}



/**
 * This class represents a task that can be executed on an ECThreadPool or
 * derived object. It's similar to an ECTask, but one can wait for the task
 * to be finished.
 */
class ECWaitableTask : public ECTask
{
public:
	static const unsigned WAIT_INFINITE = (unsigned)-1;
	
	enum State {
		Idle = 1,
		Running = 2,
		Done = 4
	};
	
public:
	virtual ~ECWaitableTask();
	virtual void execute();
	
	bool done() const;
	bool wait(unsigned timeout = WAIT_INFINITE, unsigned waitMask = Done) const;
	
protected:
	ECWaitableTask();

private:
	mutable pthread_mutex_t	m_hMutex;
	mutable pthread_cond_t	m_hCondition;
	State					m_state;
};

/**
 * Check if the task has been executed.
 * @retval true when executed, false otherwise.
 */
inline bool ECWaitableTask::done() const {
	return m_state == Done;
}


/**
 * This class can be used to run a function with one argument asynchronously on
 * an ECThreadPool or derived class.
 * To call a function with more than one argument boost::bind can be used.
 */
template<typename _Rt, typename _Fn, typename _At>
class ECDeferredFunc : public ECWaitableTask
{
public:
	/**
	 * Construct an ECDeferredFunc instance.
	 * @param[in]	fn		The function to execute
	 * @param[in]	arg		The argument to pass to fn.
	 */
	ECDeferredFunc(_Fn fn, const _At &arg) : m_fn(fn), m_arg(arg)
	{ }
	
	virtual void run() {
		m_result = m_fn(m_arg);
	}
	
	/**
	 * Get the result of the asynchronous function. This method will
	 * block until the method has been executed.
	 */
	_Rt result() const {
		wait();
		return m_result;
	}
	
private:
	_Rt	m_result;
	_Fn m_fn;
	_At m_arg;
};

#endif // ndef ECThreadPool_INCLUDED
