//=====================================================================
//
// inetcode.c - core interface of socket operation
//
// NOTE:
// for more information, please see the readme file
// 
//=====================================================================

#ifndef __INETCODE_H__
#define __INETCODE_H__

#include "inetbase.h"
#include "imemdata.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif


//---------------------------------------------------------------------
// Network Information
//---------------------------------------------------------------------
#define IMAX_HOSTNAME	256
#define IMAX_ADDRESS	64

/* host name */
extern char ihostname[];

/* host address list */
extern struct in_addr ihost_addr[];

/* host ip address string list */
extern char *ihost_ipstr[];

/* host names */
extern char *ihost_names[];

/* host address count */
extern int ihost_addr_num;	

/* refresh address list */
int inet_updateaddr(int resolvname);

/* create socketpair */
int inet_socketpair(int fds[2]);

/* keepalive option */
int inet_set_keepalive(int sock, int keepcnt, int keepidle, int keepintvl);


//---------------------------------------------------------------------
// ITMCLIENT
//---------------------------------------------------------------------
struct ITMCLIENT
{
	IUINT32 time;
	int sock;
	int state;
	long hid;
	long tag;
	int error;
	int header;
	char *buffer;
	int rc4_send_x;
	int rc4_send_y;
	int rc4_recv_x;
	int rc4_recv_y;
	unsigned char *rc4_send_box;
	unsigned char *rc4_recv_box;
	struct sockaddr local;
	struct IMSTREAM sendmsg;
	struct IMSTREAM recvmsg;
};

#ifndef ITMH_WORDLSB
#define ITMH_WORDLSB		0		// ͷ����־��2�ֽ�LSB
#define ITMH_WORDMSB		1		// ͷ����־��2�ֽ�MSB
#define ITMH_DWORDLSB		2		// ͷ����־��4�ֽ�LSB
#define ITMH_DWORDMSB		3		// ͷ����־��4�ֽ�MSB
#define ITMH_BYTELSB		4		// ͷ����־�����ֽ�LSB
#define ITMH_BYTEMSB		5		// ͷ����־�����ֽ�MSB
#define ITMH_EWORDLSB		6		// ͷ����־��2�ֽ�LSB���������Լ���
#define ITMH_EWORDMSB		7		// ͷ����־��2�ֽ�MSB���������Լ���
#define ITMH_EDWORDLSB		8		// ͷ����־��4�ֽ�LSB���������Լ���
#define ITMH_EDWORDMSB		9		// ͷ����־��4�ֽ�MSB���������Լ���
#define ITMH_EBYTELSB		10		// ͷ����־�����ֽ�LSB���������Լ���
#define ITMH_EBYTEMSB		11		// ͷ����־�����ֽ�MSB���������Լ���
#define ITMH_DWORDMASK		12		// ͷ����־��4�ֽ�LSB�������Լ������룩
#endif

#define ITMC_STATE_CLOSED		0	// ״̬���ر�
#define ITMC_STATE_CONNECTING	1	// ״̬��������
#define ITMC_STATE_ESTABLISHED	2	// ״̬�����ӽ���


void itmc_init(struct ITMCLIENT *client, struct IMEMNODE *nodes, int header);
void itmc_destroy(struct ITMCLIENT *client);

int itmc_connect(struct ITMCLIENT *client, const struct sockaddr *addr);
int itmc_assign(struct ITMCLIENT *client, int sock);
int itmc_process(struct ITMCLIENT *client);
int itmc_close(struct ITMCLIENT *client);
int itmc_status(struct ITMCLIENT *client);
int itmc_nodelay(struct ITMCLIENT *client, int nodelay);
int itmc_dsize(const struct ITMCLIENT *client);

int itmc_send(struct ITMCLIENT *client, const void *ptr, int size, int mask);
int itmc_recv(struct ITMCLIENT *client, void *ptr, int size);


int itmc_vsend(struct ITMCLIENT *client, const void *vecptr[], 
	int veclen[], int count, int mask);

int itmc_wait(struct ITMCLIENT *client, int millisec);

void itmc_rc4_set_skey(struct ITMCLIENT *client, 
	const unsigned char *key, int keylen);

void itmc_rc4_set_rkey(struct ITMCLIENT *client, 
	const unsigned char *key, int keylen);


//---------------------------------------------------------------------
// ITMHOST
//---------------------------------------------------------------------
struct ITMHOST
{
	struct IMEMNODE *nodes;
	struct IMEMNODE *cache;
	struct IMSTREAM event;
	struct sockaddr local;
	char *buffer;
	int count;
	int sock;
	int port;
	int state;
	int index;
	int limit;
	int header;
	int timeout;
	int needfree;
};

void itms_init(struct ITMHOST *host, struct IMEMNODE *cache, int header);
void itms_destroy(struct ITMHOST *host);

int itms_startup(struct ITMHOST *host, int port);
int itms_shutdown(struct ITMHOST *host);

void itms_process(struct ITMHOST *host);

void itms_send(struct ITMHOST *host, long hid, const void *data, long size);
void itms_close(struct ITMHOST *host, long hid, int reason);
void itms_settag(struct ITMHOST *host, long hid, long tag);
long itms_gettag(struct ITMHOST *host, long hid);
void itms_nodelay(struct ITMHOST *host, long hid, int nodelay);

long itms_head(struct ITMHOST *host);
long itms_next(struct ITMHOST *host, long hid);


#define ITME_NEW	0	/* wparam=hid, lparam=-1, data=sockaddr */
#define ITME_DATA	1	/* wparam=hid, lparam=tag, data=recvdata */
#define ITME_LEAVE	2	/* wparam=hid, lparam=tag, data=reason */
#define ITME_TIMER  3   /* wparam=-1, lparam=-1 */

long itms_read(struct ITMHOST *host, int *msg, long *wparam, long *lparam,
	void *data, long size);



//---------------------------------------------------------------------
// Utilies Interface
//---------------------------------------------------------------------



#ifdef __cplusplus
}
#endif



#endif



