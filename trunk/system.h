//=====================================================================
//
// system.h - system wrap for c++ 
// 
// NOTE: �����˱�Ŀ¼�¼��� C����ģ�鲢�ṩ�����ࣺ
// 
// SystemError       Exception
//
// CriticalSection   ������
// CriticalScope     ���򻥳�
// ConditionVariable ������������ƽ̨�� pthread_cond_t
// EventPosix        �¼�����ƽ̨�� Win32 Event
// ReadWriteLock     ��д��
//
// Thread            �߳̿��ƶ���
// Clock             ���ڶ�ȡ��ʱ��
// Timer             ���ڵȴ����ѵ�ʱ��
//
// KernelPoll        �첽�¼�������� libevent
// SockAddress       �׽��ֵ�ַ��IPV4��ַ����
//
// MemNode           �ڴ�ڵ㣺�̶��ڵ�������������̶��ڴ�������ã�
// MemStream         �ڴ���������ҳ�潻���� FIFO����
// RingBuffer        �����棺��״ FIFO����
// CryptRC4          RC4����
//
// CsvReader         CSV�ļ���ȡ
// CsvWriter         CSV�ļ�д��
// HttpRequest       ���� Python�� urllib��������������ģʽ
// Path              ���� Python�� os.path ·�����ӣ�����·����
//
//=====================================================================
#ifndef __SYSTEM_H__
#define __SYSTEM_H__

#include "imembase.h"
#include "imemdata.h"
#include "inetbase.h"
#include "inetcode.h"
#include "ineturl.h"
#include "iposix.h"

#include <stdio.h>
#include <assert.h>

#ifndef NAMESPACE_BEGIN
#define NAMESPACE_BEGIN(x) namespace x {
#endif

#ifndef NAMESPACE_END
#define NAMESPACE_END(x) }
#endif

#ifndef __cplusplus
#error This file must be compiled in C++ mode !!
#endif

#include <string>
#include <iostream>

NAMESPACE_BEGIN(System)

//---------------------------------------------------------------------
// Exception
//---------------------------------------------------------------------
#ifndef SYSTEM_ERROR
#define SYSTEM_ERROR

class SystemError {
public:
	SystemError(const char *what = NULL, int code = 0, int line = -1, const char *file = NULL);
	virtual ~SystemError();
	const char* what() const;
	int code() const;
	const char* file() const;
	int line() const;

protected:
	const char *_file;
	char *_what;
	int _code;
	int _line;
};

#define SYSTEM_THROW(what, code) do { \
		throw (*new ::System::SystemError(what, code, __LINE__, __FILE__)); \
	} while (0)

#ifdef _MSC_VER
#pragma warning(disable:4996)
#endif

// generate a error
inline SystemError::SystemError(const char *what, int code, int line, const char *file) {
	int size = (what)? strlen(what) : 0;
	int need = size + 2048;
	_what = new char[need];
	assert(_what);
	sprintf(_what, "%s:%d: error(%d): %s", file, line, code, what);
	fprintf(stderr, "%s\n", _what);
	fflush(stderr);
	_code = code;
	_file = file;
	_line = line;
}

// destructor of SystemError
inline SystemError::~SystemError() {
	if (_what) delete []_what;
	_what = NULL;
}

// get error message
inline const char* SystemError::what() const {
	return (_what)? _what : "";
}

// get code
inline int SystemError::code() const {
	return _code;
}

// get file
inline const char* SystemError::file() const {
	return _file;
}

// get line
inline int SystemError::line() const {
	return _line;
}

#endif


//---------------------------------------------------------------------
// ������
//---------------------------------------------------------------------
class CriticalSection
{
public:
	CriticalSection() { IMUTEX_INIT(&_mutex); }
	virtual ~CriticalSection() { IMUTEX_DESTROY(&_mutex); }
	void enter() { IMUTEX_LOCK(&_mutex); }
	void leave() { IMUTEX_UNLOCK(&_mutex); }

	IMUTEX_TYPE& mutex() { return _mutex; }
	const IMUTEX_TYPE& mutex() const { return _mutex; }

protected:
	IMUTEX_TYPE _mutex;
};


//---------------------------------------------------------------------
// �Զ���
//---------------------------------------------------------------------
class CriticalScope
{
public:
	CriticalScope(CriticalSection &c): _critical(&c) { c.enter(); }
	virtual ~CriticalScope() { _critical->leave(); }

protected:
	CriticalSection *_critical;
};



//---------------------------------------------------------------------
// ����������
//---------------------------------------------------------------------
class ConditionVariable
{
public:
	ConditionVariable() { 
		_cond = iposix_cond_new(); 
		if (_cond == NULL) 
			SYSTEM_THROW("create ConditionVariable failed", 10000);
	}

	virtual ~ConditionVariable() { 
		if (_cond) iposix_cond_delete(_cond); 
		_cond = NULL; 
	}

	void wake() { iposix_cond_wake(_cond); }
	void wake_all() { iposix_cond_wake_all(_cond); }

	bool sleep(CriticalSection &cs) { 
		return iposix_cond_sleep_cs(_cond, &cs.mutex())? true : false;
	}

	bool sleep(CriticalSection &cs, unsigned long millisec) {
		int r = iposix_cond_sleep_cs_time(_cond, &cs.mutex(), millisec);
		return r? true : false;
	}

protected:
	iConditionVariable *_cond;
};


//---------------------------------------------------------------------
// �¼�����
//---------------------------------------------------------------------
class EventPosix
{
public:
	EventPosix() { 
		_event = iposix_event_new();
		if (_event == NULL) 
			SYSTEM_THROW("create EventPosix failed", 10001);
	}

	virtual ~EventPosix() { 
		if (_event) iposix_event_delete(_event); 
		_event = NULL; 
	}

	// set signal to 1
	void set() { iposix_event_set(_event); }
	// set signal to 0
	void reset() { iposix_event_reset(_event); }

	// wait until signal is 1 (returns true), or timeout (returns false)
	bool wait(unsigned long millisec) {
		int i = iposix_event_wait(_event, millisec);
		return i == 0? false : true;
	}

	// wait infinite time
	bool wait() {
		int i= wait(IEVENT_INFINITE);
		return i == 0? false : true;
	}

protected:
	iEventPosix *_event;
};


//---------------------------------------------------------------------
// ��д��
//---------------------------------------------------------------------
class ReadWriteLock
{
public:
	ReadWriteLock() {
		_rwlock = iposix_rwlock_new();
		if (_rwlock == NULL) 
			SYSTEM_THROW("create ReadWriteLock failed", 10002);
	}

	virtual ~ReadWriteLock() {
		if (_rwlock) iposix_rwlock_delete(_rwlock);
		_rwlock = NULL;
	}

	void write_lock() { iposix_rwlock_w_lock(_rwlock); }
	void write_unlock() { iposix_rwlock_w_unlock(_rwlock); }
	void read_lock() { iposix_rwlock_r_lock(_rwlock); }
	void read_unlock() { iposix_rwlock_r_unlock(_rwlock); }

protected:
	iRwLockPosix *_rwlock;
};


//---------------------------------------------------------------------
// �߳��� Pythonģʽ
//---------------------------------------------------------------------
class Thread
{
public:
	// �����߳������ĺ�����Thread���캯��ʱ���룬��ʼ��ᱻ��������
	// ֱ�������� false/0�����ߵ����� set_notalive
	typedef int (*ThreadRunFunction)(void *parameter);

	// ���캯������������������������
	Thread(ThreadRunFunction func, void *parameter, const char *name = NULL) {
		_thread = iposix_thread_new(func, parameter, name);
		if (_thread == NULL)
			SYSTEM_THROW("create Thread failed", 10003);
	}

	virtual ~Thread() {
		if (is_running()) {
			char text[128];
			strncpy(text, "thread(", 100);
			strncat(text, iposix_thread_get_name(_thread), 100);
			strncat(text, ") is still running", 100);
			SYSTEM_THROW(text, 10010);
		}
		if (_thread) iposix_thread_delete(_thread);
		_thread = NULL;
	}

	// ��ʼ�߳�
	void start() {
		int hr = iposix_thread_start(_thread);
		if (hr != 0) {
			char text[128];
			strncpy(text, "start thread(", 100);
			strncat(text, iposix_thread_get_name(_thread), 100);
			strncat(text, ") failed", 100);
			SYSTEM_THROW(text, 10004);
		}
	}

	// �ȴ��߳̽���
	bool join(unsigned long millisec = 0xffffffff) {
		int hr = iposix_thread_join(_thread, millisec);
		if (hr != 0) return false;
		return true;
	}

	// ɱ���̣߳�Σ��
	bool kill() {
		int hr = iposix_thread_cancel(_thread);
		return hr == 0? true : false;
	}

	// ����Ϊ�ǻ
	void set_notalive() {
		iposix_thread_set_notalive(_thread);
	}

	// ����Ƿ�������
	bool is_running() const {
		return iposix_thread_is_running(_thread)? true : false;
	}

	// �߳����ȼ�
	enum ThreadPriority
	{
		PriorityLow	= 0,
		PriorityNormal = 1,
		PriorityHigh = 2,
		PriorityHighest = 3,
		PriorityRealtime = 4,
	};

	// �����߳����ȼ�����ʼ�߳�֮ǰ����
	bool set_priority(enum ThreadPriority priority) {
		return iposix_thread_set_priority(_thread, (int)priority) == 0 ? true : false;
	}

	// ����ջ��С����ʼ�߳�ǰ���ã�Ĭ�� 1024 * 1024
	bool set_stack(int stacksize) {
		return iposix_thread_set_stack(_thread, stacksize) == 0? true : false;
	}

	// �������е� cpu�������ǿ�ʼ�̺߳�����
	bool set_affinity(unsigned int cpumask) {
		return iposix_thread_affinity(_thread, cpumask) == 0? true : false;
	}

	// �����ź�
	void set_signal(int sig) {
		iposix_thread_set_signal(_thread, sig);
	}

	// ȡ���ź�
	int get_signal() {
		return iposix_thread_get_signal(_thread);
	}

	// ȡ������
	const char *get_name() const {
		return iposix_thread_get_name(_thread);
	}

	// ����Ϊ�߳��ڲ����õľ�̬��Ա

	// ȡ�õ�ǰ�߳�����
	static const char *CurrentName() {
		return iposix_thread_get_name(NULL);
	}

	// ȡ�õ�ǰ�߳��ź�
	static int CurrentSignal() { 
		return iposix_thread_get_signal(NULL); 
	}

	// ���õ�ǰ�߳��ź�
	static void SetCurrentSignal(int sig) {
		iposix_thread_set_signal(NULL, sig);
	}

protected:
	iPosixThread *_thread;
};



//---------------------------------------------------------------------
// ʱ�亯��
//---------------------------------------------------------------------
class Clock
{
public:
	// ȡ�� 32λ�ĺ��뼶��ʱ��
	static inline IUINT32 GetInMs() { return iclock(); }

	// ȡ�� 64λ�ĺ��뼶��ʱ��
	static IUINT64 GetTick() { return iclock64(); }	// millisec

	// ȡ�� 64λ��΢�뼶��ʱ�ӣ�1΢��=1/1000000�룩
	static IUINT64 GetRealTime() { return iclockrt(); }	// usec
};


//---------------------------------------------------------------------
// ʱ�ӿ���
//---------------------------------------------------------------------
class Timer
{
public:
	Timer() {
		_timer = iposix_timer_new();
		if (_timer == NULL) 
			SYSTEM_THROW("create Timer failed", 10004);
	}

	virtual ~Timer() {
		if (_timer) iposix_timer_delete(_timer);
		_timer = NULL;
	}

	// ��ʼʱ��
	bool start(unsigned long delay, bool periodic = true) {
		return iposix_timer_start(_timer, delay, periodic? 1 : 0) == 0 ? true : false;
	}
	// ����ʱ��
	void stop() {
		iposix_timer_stop(_timer);
	}

	// �ȴ���Ĭ�����޵ȴ�����λ����
	bool wait(unsigned long timeout = 0xffffffff) {
		if (timeout == 0xffffffff) {
			return iposix_timer_wait(_timer)? true : false;
		}	else {
			return iposix_timer_wait_time(_timer, timeout)? true : false;
		}
	}

protected:
	iPosixTimer *_timer;
};


//---------------------------------------------------------------------
// �첽�¼� Polling���̲߳���ȫ
//---------------------------------------------------------------------
class KernelPoll
{
public:
	KernelPoll() { 
		ipoll_init(IDEVICE_AUTO);
		int retval = ipoll_create(&_ipoll_desc, 2000);
		if (retval == 0) {
			SYSTEM_THROW("error to create poll descriptor", 10003);
		}
	}

	virtual ~KernelPoll() {
		if (_ipoll_desc) {
			ipoll_delete(_ipoll_desc);
			_ipoll_desc = NULL;
		}
	}

	// ����һ���׽��֣�mask�ǳ�ʼ�¼����룺IPOLL_IN(1) / IPOLL_OUT(2) / IPOLL_ERR(4) �����
	// udata������¼���Ӧ�Ķ���ָ�룬���ڴ� eventȡ���¼�ʱ���ָ�����Ӧȡ��
	int add(int fd, int mask, void *udata) { return ipoll_add(_ipoll_desc, fd, mask, udata); }

	// ɾ��һ���׽��֣����ٽ����������¼�
	int del(int fd) { return ipoll_del(_ipoll_desc, fd); }

	// �����׽��ֵ��¼����룺IPOLL_IN(1) / IPOLL_OUT(2) / IPOLL_ERR(4) �����
	int set(int fd, int mask) { return ipoll_set(_ipoll_desc, fd, mask); }
	
	// �ȴ����ٺ��룬ֱ�����¼�������millisecΪ0�Ļ��Ͳ�����ֱ�ӷ���
	// �����ж��ٸ��׽����Ѿ����¼��ˡ�
	int wait(int millisec) { return ipoll_wait(_ipoll_desc, millisec); }

	// ȡ���¼����������ã�ֱ������ false
	bool event(int *fd, int *event, void **udata) { 
		int retval = ipoll_event(_ipoll_desc, fd, event, udata);
		return (retval == 0)? true : false;
	}

protected:
	ipolld _ipoll_desc;
};


//---------------------------------------------------------------------
// �����ַ��IPV4
//---------------------------------------------------------------------
class SockAddress
{
public:
	SockAddress() { isockaddr_set(&_remote, 0, 0); }

	SockAddress(const char *ip, int port) { set(ip, port); }

	SockAddress(unsigned long ip, int port) { set(ip, port); }

	void set(const char *ip, int port) { isockaddr_makeup(&_remote, ip, port); }

	void set(unsigned long ip, int port) { isockaddr_set(&_remote, ip, port); }

	void set_family(int family) { isockaddr_set_family(&_remote, family); }

	void set_ip(unsigned long ip) { isockaddr_set_ip(&_remote, ip); }

	void set_ip(const char *ip) { isockaddr_set_ip_text(&_remote, ip); }

	void set_port(int port) { isockaddr_set_port(&_remote, port); }

	unsigned long get_ip() const { return isockaddr_get_ip(&_remote); }

	int get_port() const { return isockaddr_get_port(&_remote); }

	int get_family() const { return isockaddr_get_family(&_remote); }

	const sockaddr* address() const { return &_remote; }

	sockaddr* address() { return &_remote; }

	char *string(char *out) { return isockaddr_str(&_remote, out); }

	void trace(std::ostream & os) { char text[32]; os << string(text); }


protected:
	sockaddr _remote;
};


//---------------------------------------------------------------------
// �ڴ�ڵ������
//---------------------------------------------------------------------
class MemNode
{
public:
	// ��ʼ���ڵ������������ÿ��node�Ĵ�С�������������ƣ����һ������ϵͳ����
	// ������ٸ��ڵ㣩��
	MemNode(int nodesize, int growlimit = 1024) {
		_node = imnode_create(nodesize, growlimit);
		if (_node == NULL) {
			SYSTEM_THROW("Error to create imemnode_t", 10004);
		}
		_nodesize = nodesize;
	}

	virtual ~MemNode() { imnode_delete(_node); _node = NULL; }

	// ����һ���½ڵ�
	ilong new_node() { return imnode_new(_node); }
	
	// ɾ��һ���ѷ���ڵ�
	void delete_node(ilong id) { imnode_del(_node, id); }

	// ȡ�ýڵ�����Ӧ�Ķ���ָ��
	const void *node(ilong id) const { return IMNODE_DATA(_node, id); }

	// ȡ�ýڵ�����Ӧ�Ķ���ָ��
	void *node(ilong id) { return IMNODE_DATA(_node, id); }

	// ȡ�ýڵ�����С
	int node_size() const { return _nodesize; }

	// ��һ���ѷ���ڵ㣬<0 Ϊû�нڵ�
	ilong head() { return imnode_head(_node); }

	// ��һ���Ѿ�����Ľڵ㣬<0 Ϊ����
	ilong next(ilong id) { return imnode_next(_node, id); }

	// ��һ���Ѿ�����ڵ㣬<0 Ϊ��
	ilong prev(ilong id) { return imnode_prev(_node, id); }

	// ȡ�ýڵ������ Cԭʼ����
	imemnode_t* node_ptr() { return _node; }

	const imemnode_t* node_ptr() const { return _node; }

protected:
	int _nodesize;
	imemnode_t *_node;
};


//---------------------------------------------------------------------
// �ڴ���
//---------------------------------------------------------------------
class MemStream
{
public:
	MemStream(imemnode_t *node) {
		ims_init(&_stream, node, -1, -1);
	}

	MemStream(MemNode *node) {
		ims_init(&_stream, node? node->node_ptr() : NULL, -1, -1);
	}

	MemStream(ilong low = -1, ilong high = -1) {
		ims_init(&_stream, NULL, low, high);
	}

	virtual ~MemStream() { ims_destroy(&_stream); }

	// ȡ�����ݴ�С
	ilong size() const { return ims_dsize(&_stream); }

	// д������
	ilong write(const void *data, ilong size) { return ims_write(&_stream, data, size); }

	// ��ȡ���ݣ����ӻ���������Ѷ�����
	ilong read(void *data, ilong size) { return ims_read(&_stream, data, size); }

	// ��ȡ���ݵ����ƶ���ָ�루���ݲ�������
	ilong peek(void *data, ilong size) { return ims_peek(&_stream, data, size); }

	// ��������
	ilong drop(ilong size) { return ims_drop(&_stream, size); }

	// �������
	void clear() { ims_clear(&_stream); }

	// ȡ�õ�ǰ������һƬ���ݵ�ַָ�룬�������������ݵĴ�С
	ilong flat(void **ptr) { return ims_flat(&_stream, ptr); }

protected:
	IMSTREAM _stream;
};



//---------------------------------------------------------------------
// ��״����
//---------------------------------------------------------------------
class RingBuffer
{
public:
	// ��ʼ��������
	RingBuffer(void *ptr, ilong size) { iring_init(&_ring, ptr, size); }

	// ȡ�����ݴ�С
	ilong size() { return iring_dsize(&_ring); }

	// ȡ�û����ԷŶ�������
	ilong space() { return iring_fsize(&_ring); }

	// д������
	ilong write(const void *ptr, ilong size) { return iring_write(&_ring, ptr, size); }

	// ��ȡ���ݣ����ӻ���������Ѷ�����
	ilong read(void *ptr, ilong size) { return iring_read(&_ring, ptr, size); }

	// ��ȡ���ݵ����ƶ���ָ�루���ݲ�������
	ilong peek(void *ptr, ilong size) { return iring_peek(&_ring, ptr, size); }

	// ��������
	ilong drop(ilong size) { return iring_drop(&_ring, size); }

	// �������
	void clear() { iring_clear(&_ring); }

	// ȡ�õ�ǰ������һƬ���ݵ�ַָ�룬�������������ݵĴ�С
	ilong flat(void **ptr) { return iring_flat(&_ring, ptr); }

	// ���ض�ƫ�Ʒ�������
	ilong put(ilong pos, const void *data, ilong size) { return iring_put(&_ring, pos, data, size); }

	// ���ض�ƫ��ȡ������
	ilong get(ilong pos, void *data, ilong size) { return iring_get(&_ring, pos, data, size); }

	// �����ռ䣨�������ݺ�ָ�룩
	bool swap(void *ptr, ilong size) { return iring_swap(&_ring, ptr, size)? false : true; }

	// ������βָ������ݴ�С
	ilong ring_ptr(char **p1, ilong *s1, char **p2, ilong *s2) { return iring_ptr(&_ring, p1, s1, p2, s2); }

protected:
	IRING _ring;
};


//---------------------------------------------------------------------
// RC4 Crypt
//---------------------------------------------------------------------
class CryptRC4
{
public:
	CryptRC4(const unsigned char *key, int size) {
		_box = new unsigned char[256];
		if (_box == NULL) {
			SYSTEM_THROW("error to alloc rc4 box", 10004);
		}
		icrypt_rc4_init(_box, &x, &y, key, size);
	}

	virtual ~CryptRC4() {
		delete _box;
		_box = NULL;
	}

	// ����
	void *crypt(const void *src, long size, void *dst) {
		icrypt_rc4_crypt(_box, &x, &y, (const unsigned char*)src, 
			(unsigned char*)dst, size);
		return dst;
	}

protected:
	unsigned char *_box;
	int x;
	int y;
};



//---------------------------------------------------------------------
// URL �����װ
//---------------------------------------------------------------------
class HttpRequest
{
public:
	HttpRequest() { _urld = NULL; }
	virtual ~HttpRequest() { close(); }

	// ��һ��URL
	// POST mode: size >= 0 && data != NULL 
	// GET mode: size < 0 || data == NULL
	// proxy format: a string: (type, addr, port [,user, passwd]) join by "\n"
	// NULL for direct link. 'type' can be one of 'http', 'socks4' and 'socks5', 
	// eg: type=http, proxyaddr=10.0.1.1, port=8080 -> "http\n10.0.1.1\n8080"
	// eg: "socks5\n10.0.0.1\n80\nuser1\npass1" "socks4\n127.0.0.1\n1081"
	bool open(const char *URL, const void *data = NULL, long size = -1, 
		const char *header = NULL, const char *proxy = NULL, int *errcode = NULL) {
		close();
		_urld = ineturl_open(URL, data, size, header, proxy, errcode); 
		return (_urld == NULL)? false : true;
	}

	// �ر�����
	void close() { 
		if (_urld != NULL) ineturl_close(_urld);
		_urld = NULL; 
	}

	// ��ȡ���ݣ�����ֵ���£�
	// returns IHTTP_RECV_AGAIN for block
	// returns IHTTP_RECV_DONE for okay
	// returns IHTTP_RECV_CLOSED for closed
	// returns IHTTP_RECV_NOTFIND for not find
	// returns IHTTP_RECV_ERROR for http error
	// returns > 0 for received data size
	long read(void *ptr, long size, int waitms) { 
		if (_urld == NULL) return -1000; 
		return ineturl_read(_urld, ptr, size, waitms);
	}

	// writing extra post data
	// returns data size in send-buffer;
	long write(const void *ptr, long size) {
		if (_urld == NULL) return -1000; 
		return ineturl_write(_urld, ptr, size);
	}

	// flush: try to send data from buffer to network
	void flush() {
		if (_urld) {
			ineturl_flush(_urld);
		}
	}

	// ����һ��Զ�� URL������������� content
	// returns >= 0 for okay, below zero for errors:
	// returns IHTTP_RECV_CLOSED for closed
	// returns IHTTP_RECV_NOTFIND for not find
	// returns IHTTP_RECV_ERROR for http error
	static inline int wget(const char *url, std::string &content, const char *proxy = NULL, int timeout = 8000) {
		ivalue_t ctx;
		int hr;
		it_init(&ctx, ITYPE_STR);
		hr = _urllib_wget(url, &ctx, proxy, timeout);
		content.assign(it_str(&ctx), it_size(&ctx));
		it_destroy(&ctx);
		return hr;
	}

protected:
	IURLD *_urld;
};


//---------------------------------------------------------------------
// CSV READER
//---------------------------------------------------------------------
class CsvReader
{
public:
	CsvReader() {
		_reader = NULL;
		_index = 0;
		_count = 0;
		_readed = false;
	}

	virtual ~CsvReader() {
		close();
	}

	void close() {
		if (_reader) icsv_reader_close(_reader);
		_reader = NULL;
		_index = 0;
		_count = 0;
		_readed = false;
	}

	// �� CSV�ļ�
	bool open(const char *filename) {
		close();
		_reader = icsv_reader_open_file(filename);
		if (_reader == NULL) return false;
		_readed = false;
		return true;
	}

	// ���ڴ�
	bool open(const char *text, ilong size) {
		close();
		_reader = icsv_reader_open_memory(text, size);
		if (_reader == NULL) return false;
		_readed = false;
		return true;
	}

	// ��ȡһ�У����ض�����
	int read() {
		if (_reader == NULL) return -1;
		int retval = icsv_reader_read(_reader);
		if (retval >= 0) _count = retval;
		else _count = 0;
		_index = 0;
		_readed = true;
		return retval;
	}

	// �����ж�����
	int size() const { 
		return _count;
	}

	// �ж��Ƿ��ļ�����
	bool eof() const {
		if (_reader == NULL) return true;
		return icsv_reader_eof(_reader)? true : false;
	}

	// ��������
	CsvReader& operator >> (char *ptr) { get(_index++, ptr, 1024); return *this; }
	CsvReader& operator >> (std::string &str) { get(_index++, str); return *this; }
	CsvReader& operator >> (ivalue_t *str) { get(_index++, str); return *this; }
	CsvReader& operator >> (int &value) { get(_index++, value); return *this; }
	CsvReader& operator >> (unsigned int &value) { get(_index++, value); return *this; }
	CsvReader& operator >> (long &value) { get(_index++, value); return *this; }
	CsvReader& operator >> (unsigned long &value) { get(_index++, value); return *this; }
	CsvReader& operator >> (IINT64 &value) { get(_index++, value); return *this; }
	CsvReader& operator >> (IUINT64 &value) { get(_index++, value); return *this; }
	CsvReader& operator >> (float &value) { get(_index++, value); return *this; }
	CsvReader& operator >> (double &value) { get(_index++, value); return *this; }
	CsvReader& operator >> (const void *p) { if (p == NULL) read(); return *this; }

	// �и�λ
	void reset() { _index = 0; }

	bool get(int pos, char *ptr, int size) {
		return icsv_reader_get_cstr(_reader, pos, ptr, size) >= 0? true : false;
	}

	bool get(int pos, ivalue_t *str) {
		return icsv_reader_get_string(_reader, pos, str) >= 0? true : false;
	}

	bool get(int pos, std::string& str) {
		const ivalue_t *src = icsv_reader_get_const(_reader, pos);
		if (src == NULL) {
			str.assign("");
			return false;
		}
		str.assign(it_str(src), (size_t)it_size(src));
		return true;
	}

	bool get(int pos, long &value) {
		return icsv_reader_get_long(_reader, pos, &value) < 0? false : true;
	}

	bool get(int pos, unsigned long &value) {
		return icsv_reader_get_ulong(_reader, pos, &value) < 0? false : true;
	}

	bool get(int pos, IINT64 &value) {
		return icsv_reader_get_int64(_reader, pos, &value) < 0? false : true;
	}

	bool get(int pos, IUINT64 &value) {
		return icsv_reader_get_uint64(_reader, pos, &value) < 0? false : true;
	}

	bool get(int pos, int &value) {
		return icsv_reader_get_int(_reader, pos, &value) < 0? false : true;
	}

	bool get(int pos, unsigned int &value) {
		return icsv_reader_get_uint(_reader, pos, &value) < 0? false : true;
	}

	bool get(int pos, float &value) {
		return icsv_reader_get_float(_reader, pos, &value) < 0? false : true;
	}

	bool get(int pos, double &value) {
		return icsv_reader_get_double(_reader, pos, &value) < 0? false : true;
	}

protected:
	iCsvReader *_reader;
	int _index;
	int _count;
	bool _readed;
};


#define CsvNextRow ((const void*)0)


//---------------------------------------------------------------------
// CSV WRITER
//---------------------------------------------------------------------
class CsvWriter
{
public:
	CsvWriter() {
		_writer = NULL;
	}

	virtual ~CsvWriter() {
		close();
	}

	void close() {
		if (_writer) {
			icsv_writer_close(_writer);
			_writer = NULL;
		}
	}

	bool open(const char *filename, bool append = false) {
		close();
		_writer = icsv_writer_open(filename, append? 1 : 0);
		return _writer? true : false;
	}

	bool write() {
		if (_writer == NULL) return false;
		return icsv_writer_write(_writer) == 0? true : false;
	}

	int size() const {
		if (_writer == NULL) return 0;
		return icsv_writer_size(_writer);
	}

	void clear() {
		if (_writer) icsv_writer_clear(_writer);
	}

	void empty() {
		if (_writer) icsv_writer_empty(_writer);
	}

	CsvWriter& operator << (const char *src) { push(src); return *this; }
	CsvWriter& operator << (const std::string &src) { push(src); return *this; }
	CsvWriter& operator << (long value) { push(value); return *this; }
	CsvWriter& operator << (unsigned long value) { push(value); return *this; }
	CsvWriter& operator << (int value) { push(value); return *this; }
	CsvWriter& operator << (unsigned int value) { push(value); return *this; }
	CsvWriter& operator << (IINT64 value) { push(value); return *this; }
	CsvWriter& operator << (IUINT64 value) { push(value); return *this; }
	CsvWriter& operator << (float value) { push(value); return *this; }
	CsvWriter& operator << (double value) { push(value); return *this; }
	CsvWriter& operator << (const void *ptr) { if (ptr == NULL) { write(); } return *this; }

	void push(const char *src, ilong size = -1) { 
		icsv_writer_push_cstr(_writer, src, size); 
	}

	void push(const std::string &src) { 
		icsv_writer_push_cstr(_writer, src.c_str(), (ilong)src.size());
	}

	void push(const ivalue_t *src) {
		icsv_writer_push_cstr(_writer, it_str(src), (int)it_size(src));
	}

	void push(long value, int radix = 10) {
		icsv_writer_push_long(_writer, value, radix);
	}

	void push(unsigned long value, int radix = 10) {
		icsv_writer_push_ulong(_writer, value, radix);
	}

	void push(int value, int radix = 10) {
		icsv_writer_push_int(_writer, value, radix);
	}

	void push(unsigned int value, int radix = 10) {
		icsv_writer_push_uint(_writer, value, radix);
	}

	void push(IINT64 value, int radix = 10) {
		icsv_writer_push_int64(_writer, value, radix);
	}

	void push(IUINT64 value, int radix = 10) {
		icsv_writer_push_uint64(_writer, value, radix);
	}

	void push(float f) {
		icsv_writer_push_float(_writer, f);
	}

	void push(double f) {
		icsv_writer_push_double(_writer, f);
	}

protected:
	iCsvWriter *_writer;
};


//---------------------------------------------------------------------
// netstream
//---------------------------------------------------------------------
class NetStream
{
public:
	// ͷ����ʽ����
	enum HeaderMode
	{
		HEADER_WORDLSB		= 0,	// ͷ����־��2�ֽ�LSB
		HEADER_WORDMSB		= 1,	// ͷ����־��2�ֽ�MSB
		HEADER_DWORDLSB		= 2,	// ͷ����־��4�ֽ�LSB
		HEADER_DWORDMSB		= 3,	// ͷ����־��4�ֽ�MSB
		HEADER_BYTELSB		= 4,	// ͷ����־�����ֽ�LSB
		HEADER_BYTEMSB		= 5,	// ͷ����־�����ֽ�MSB
		HEADER_EWORDLSB		= 6,	// ͷ����־��2�ֽ�LSB���������Լ���
		HEADER_EWORDMSB		= 7,	// ͷ����־��2�ֽ�MSB���������Լ���
		HEADER_EDWORDLSB	= 8,	// ͷ����־��4�ֽ�LSB���������Լ���
		HEADER_EDWORDMSB	= 9,	// ͷ����־��4�ֽ�MSB���������Լ���
		HEADER_EBYTELSB		= 10,	// ͷ����־�����ֽ�LSB���������Լ���
		HEADER_EBYTEMSB		= 11,	// ͷ����־�����ֽ�MSB���������Լ���
		HEADER_DWORDMASK	= 12,	// ͷ����־��4�ֽ�LSB�������Լ������룩
	};

	// ����״̬
	enum NetState
	{
		STATE_CLOSED = 0,
		STATE_CONNECTING = 1,
		STATE_ESTABLISHED = 2,
	};

public:
	virtual ~NetStream() {
		CriticalScope scope_lock(lock);
		itmc_destroy(&client);
	}

	NetStream(HeaderMode header = HEADER_WORDLSB) {
		CriticalScope scope_lock(lock);
		itmc_init(&client, NULL, (int)header);
	}

	int connect(unsigned long ip, int port) {
		CriticalScope scope_lock(lock);
		sockaddr remote;
		isockaddr_set(&remote, ip, port);
		return itmc_connect(&client, &remote);
	}

	int connect(const char *ip, int port) {
		CriticalScope scope_lock(lock);
		sockaddr remote;
		isockaddr_makeup(&remote, ip, port);
		return itmc_connect(&client, &remote);
	}

	int connect(const sockaddr *remote) {
		CriticalScope scope_lock(lock);
		return itmc_connect(&client, remote);
	}

	int assign(int sock) {
		CriticalScope scope_lock(lock);
		return itmc_assign(&client, sock);
	}

	void close() {
		CriticalScope scope_lock(lock);
		itmc_close(&client);
	}

	void process() {
		CriticalScope scope_lock(lock);
		itmc_process(&client);
	}

	NetState status() const {
		return (NetState)client.state;
	}

	void nodelay(bool nodelay) {
		CriticalScope scope_lock(lock);
		itmc_nodelay(&client, nodelay? 1 : 0);
	}

	int wait(int millisec = 0) {
		CriticalScope scope_lock(lock);
		return itmc_wait(&client, millisec);
	}

	int size() {
		CriticalScope scope_lock(lock);
		return itmc_dsize(&client);
	}

	int send(const void *ptr, int size, int mask = 0) {
		CriticalScope scope_lock(lock);
		return itmc_send(&client, ptr, size, mask);
	}

	int recv(void *ptr, int size) {
		CriticalScope scope_lock(lock);
		return itmc_recv(&client, ptr, size);
	}

	int send(const void *vecptr[], int veclen[], int count, int mask = 0) {
		CriticalScope scope_lock(lock);
		return itmc_vsend(&client, vecptr, veclen, count, mask);
	}

protected:
	ITMCLIENT client;
	CriticalSection lock;
};


//---------------------------------------------------------------------
// Posix �ļ�����
//---------------------------------------------------------------------
#ifndef IDISABLE_FILE_SYSTEM_ACCESS

class Path {

public:

	// ȡ�þ���·��
	static inline bool Absolute(const char *path, std::string &output) {
		char buffer[IPOSIX_MAXBUFF];
		if (iposix_path_abspath(path, buffer, IPOSIX_MAXPATH) == NULL) {
			output.assign("");
			return false;
		}
		output.assign(buffer);
		return true;
	}

	// ��һ��·��
	static inline bool Normalize(const char *path, std::string &output) {
		char buffer[IPOSIX_MAXBUFF];
		if (iposix_path_normal(path, buffer, IPOSIX_MAXPATH) == NULL) {
			output.assign("");
			return false;
		}
		output.assign(buffer);
		return true;
	}

	// ����·��
	static inline bool Join(const char *p1, const char *p2, std::string &output) {
		char buffer[IPOSIX_MAXBUFF];
		if (iposix_path_join(p1, p2, buffer, IPOSIX_MAXPATH) == NULL) {
			output.assign("");
			return false;
		}
		output.assign(buffer);
		return true;
	}

	// �з�·��Ϊ��·�� + �ļ���
	static inline bool Split(const char *path, std::string &dir, std::string &file) {
		char buf1[IPOSIX_MAXBUFF];
		char buf2[IPOSIX_MAXBUFF];
		if (iposix_path_split(path, buf1, IPOSIX_MAXPATH, buf2, IPOSIX_MAXPATH) != 0) {
			dir.assign("");
			file.assign("");
			return false;
		}
		dir.assign(buf1);
		file.assign(buf2);
		return true;
	}

	// �ָ���չ��
	static inline bool SplitExt(const char *path, std::string &p1, std::string &p2) {
		char buf1[IPOSIX_MAXBUFF];
		char buf2[IPOSIX_MAXBUFF];
		if (iposix_path_splitext(path, buf1, IPOSIX_MAXPATH, buf2, IPOSIX_MAXPATH) != 0) {
			p1.assign("");
			p2.assign("");
			return false;
		}
		p1.assign(buf1);
		p2.assign(buf2);
		return true;
	}

	// ȡ�ÿ�ִ���ļ�·��
	static inline const char *GetProcPath() {
		return iposix_get_exepath();
	}

	// ȡ�ÿ�ִ���ļ�Ŀ¼
	static inline const char *GetProcDir() {
		return iposix_get_execwd();
	}
};

#endif


NAMESPACE_END(System)


#endif


