#ifndef __F_ATHEOS_NET_H__
#define __F_ATHEOS_NET_H__

#ifdef __KERNEL__

#include <net/nettypes.h>
#include <atheos/socket.h>
#include <net/in.h>
#include <net/if.h>
#include <net/if_ether.h>

#define __BYTE(v,n) ((((uint32)v) >> (n*8))&0xff)
#define htonw(n) ( (__BYTE(n, 0 ) << 8) | __BYTE(n, 1 ) )
#define ntohw(n) ( (__BYTE(n, 0 ) << 8) | __BYTE(n, 1 ) )

/*
 *	This is a version of ip_compute_csum() optimized for IP headers,
 *	which always checksum on 4 octet boundaries.
 *
 *	By Jorge Cwik <jorge@laser.satlink.net>, adapted for linux by
 *	Arnt Gulbrandsen.
 *
 *	Adapted to AtheOS by Kurt Skauen.
 */

static inline uint16 ip_fast_csum( uint8 * iph, uint32 ihl )
{
  uint32 sum;

  __asm__ __volatile__("
	    movl (%1), %0
	    subl $4, %2
	    jbe 2f
	    addl 4(%1), %0
	    adcl 8(%1), %0
	    adcl 12(%1), %0
1:	    adcl 16(%1), %0
	    lea 4(%1), %1
	    decl %2
	    jne	1b
	    adcl $0, %0
	    movl %0, %2
	    shrl $16, %0
	    addw %w2, %w0
	    adcl $0, %0
	    notl %0
2:
	    "
	/* Since the input registers which are loaded with iph and ipl
	   are modified, we must also specify them as outputs, or gcc
	   will assume they contain their original values. */
	: "=r" (sum), "=r" (iph), "=r" (ihl)
	: "1" (iph), "2" (ihl));
  return(sum);
}


typedef struct _UdpPort UdpPort_s;
struct _UdpPort
{
    UdpPort_s*	up_psNext;
    NetQueue_s	up_sPackets;
    int		up_nPort;
};


#define MAX_NET_INTERFACES 64




struct _ArpEntry
{
    ArpEntry_s*	ae_psNext;
    ipaddr_t	ae_anIPAddress;
    bigtime_t	ae_nExpieringTime;
    uint8	ae_anHardwareAddr[IH_MAX_HWA_LEN];
    NetQueue_s	ae_sPackets;
    bool	ae_bIsValid;
};


void init_ip();
void init_arp();
int  init_net_core();


enum
{
  NCFG_SET_DEFAULT_ADDRESS,
  NCFG_SET_ADDRESS,
};

struct ifc_set_address
{
  struct sockaddr address;
  struct sockaddr netmask;
};

int net_config( const char* pzDevice, int nCmd, void* pArg );

int config_net_dev( const char* pzDev, const struct sockaddr* psAddr, const struct sockaddr* psNetMask );
int sys_config_net_dev( const char* pzDev, const struct sockaddr* psAddr, const struct sockaddr* psNetMask );


int create_socket( bool bKernel, int nFamily, int nType, int nProtocol, bool bInitProtocol, Socket_s** ppsRes );

void ip_in( PacketBuf_s* psPkt, int nPktSize );
void tcp_in( PacketBuf_s* psPkt, int nDataLen );
void udp_in( PacketBuf_s* psPkt, int nDataLen );

void format_ipaddress( char* pzBuffer, ipaddr_t pAddress );
int parse_ipaddress( ipaddr_t pAddress, const char* pzBuffer );

Route_s*     ip_find_route( ipaddr_t pDstAddr );
void 	     ip_release_route( Route_s* psRoute );

PacketBuf_s* alloc_pkt_buffer( int nSize );
void	     free_pkt_buffer( PacketBuf_s* psBuf );
void	     reserve_pkt_header( PacketBuf_s* psBuf, int nSize );

int 	     init_net_queue( NetQueue_s* psQueue );
void	     delete_net_queue( NetQueue_s* psQueue );
void 	     enqueue_packet( NetQueue_s* psQueue, PacketBuf_s* psBuf );
PacketBuf_s* remove_head_packet( NetQueue_s* psQueue, bigtime_t nTimeout );

Route_s* add_route( ipaddr_t pNetAddr, ipaddr_t pNetMask, ipaddr_t pGateway, int nMetric, NetInterface_s* psInterface );

void init_net_interfaces(void);
//void probe_eth_interfaces();
int add_net_interface( NetInterface_s* psInterface );
NetInterface_s* get_net_interface( int nIndex );
int send_packet( Route_s* psRoute, ipaddr_t pDstAddress, PacketBuf_s* psPkt );
void dispatch_net_packet( PacketBuf_s* psPkt );

int ip_send( PacketBuf_s* psPkt );


NetInterface_s* find_interface( const char* pzName );
NetInterface_s* find_interface_by_addr( ipaddr_t anAddress );

int netif_ioctl( int nCmd, void* pBuf );

/*
int get_interface_config( struct ifconf* psConf );
int get_interface_flags( struct ifreq* psConf );
int get_interface_address( struct ifreq* psConf );
int set_interface_address( struct ifreq* psConf );
int get_interface_netmask( struct ifreq* psConf );
int set_interface_netmask( struct ifreq* psConf );
*/
#endif /* __KERNEL__ */

#endif /* __F_ATHEOS_NET_H__ */
