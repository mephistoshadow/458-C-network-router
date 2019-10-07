/**********************************************************************
 * file:  sr_router.c
 * date:  Mon Feb 18 12:50:42 PST 2002
 * Contact: casado@stanford.edu
 *
 * Description:
 *
 * This file contains all the functions that interact directly
 * with the routing table, as well as the main entry method
 * for routing.
 *
 **********************************************************************/



#include <stdlib.h>
#include  <string.h>

#include <stdio.h>
#include <assert.h>


#include "sr_if.h"
#include "sr_rt.h"
#include "sr_router.h"
#include "sr_protocol.h"
#include "sr_arpcache.h"
#include "sr_utils.h"




/*---------------------------------------------------------------------
 * Method: sr_init(void)
 * Scope:  Global
 *
 * Initialize the routing subsystem
 *
 *---------------------------------------------------------------------*/

void sr_init(struct sr_instance* sr)
{
    /* REQUIRES */
  assert(sr);

    /* Initialize cache and cache cleanup thread */
  sr_arpcache_init(&(sr->cache));

  pthread_attr_init(&(sr->attr));
  pthread_attr_setdetachstate(&(sr->attr), PTHREAD_CREATE_JOINABLE);
  pthread_attr_setscope(&(sr->attr), PTHREAD_SCOPE_SYSTEM);
  pthread_attr_setscope(&(sr->attr), PTHREAD_SCOPE_SYSTEM);
  pthread_t thread;

  pthread_create(&thread, &(sr->attr), sr_arpcache_timeout, sr);

    /* Add initialization code here! */

} /* -- sr_init -- */

/*---------------------------------------------------------------------
 * Method: sr_handlepacket(uint8_t* p,char* interface)
 * Scope:  Global
 *
 * This method is called each time the router receives a packet on the
 * interface.  The packet buffer, the packet length and the receiving
 * interface are passed in as parameters. The packet is complete with
 * ethernet headers.
 *
 * Note: Both the packet buffer and the character's memory are handled
 * by sr_vns_comm.c that means do NOT delete either.  Make a copy of the
 * packet instead if you intend to keep it around beyond the scope of
 * the method call.
 *
 *---------------------------------------------------------------------*/

  void sr_handlepacket(struct sr_instance* sr,uint8_t * packet,unsigned int len,char* interface){
  /* REQUIRES */
    assert(sr);
    assert(packet);
    assert(interface);


    printf("*** -> Received packet of length %d \n",len);
    sr_ethernet_hdr_t *ethernet_hdr = (sr_ethernet_hdr_t*) packet;
    if (len != sizeof(sr_ethernet_hdr_t)) {
      printf("the lengh does not meet the minimum length of ethernet\n");

    }

    uint16_t e_type = ethertype((uint8_t *)ethernet_hdr);
    if(e_type == ethertype_ip) {
      
    } else if (e_type == ethertype_arp) {
      handle_arp_total(sr,packet,len,interface);
    }

  }
  
  void handle_arp_total(struct sr_instance* sr,uint8_t * packet,unsigned int len,char* interface){
   sr_arp_hdr_t *arp_hdr = (sr_arp_hdr_t*) (packet + sizeof(sr_ethernet_hdr_t));
   if(arp_hdr ->ar_op == arp_op_request) {
    arp_request(sr,packet,len,interface);
  }else if(arp_hdr ->ar_op == arp_op_reply) {
   handle_arp_reply(sr,packet,len,interface);
 }

}

void arp_request(struct sr_instance* sr,uint8_t * packet,unsigned int len,char* interface){
  struct sr_if *sr_interface = sr_get_interface(sr,interface);

    sr_ethernet_hdr_t *ethernet_hdr = (sr_ethernet_hdr_t*) packet;
    sr_arp_hdr_t *arp_hdr = (sr_arp_hdr_t*) (packet + sizeof(sr_ethernet_hdr_t));
      /* get the new packet length.*/
    int length_new_packet = sizeof(sr_ethernet_hdr_t) + sizeof(sr_arp_hdr_t);
       /* malloc the send back packet memory.*/
    uint8_t  *back_packet = (uint8_t*)malloc(length_new_packet);
       /* assign the new thernet header.*/
    sr_ethernet_hdr_t* new_ethernet_hdr = (sr_ethernet_hdr_t*) back_packet;
    memcpy(new_ethernet_hdr -> ether_dhost,ethernet_hdr -> ether_shost,ETHER_ADDR_LEN);
    memcpy(new_ethernet_hdr -> ether_shost,sr_interface -> addr,ETHER_ADDR_LEN);
    new_ethernet_hdr -> ether_type = ethernet_hdr -> ether_type;
       /* assign the new arp header for send back.*/
    sr_arp_hdr_t *new_arp_hdr = (sr_arp_hdr_t*) (back_packet + sizeof(sr_ethernet_hdr_t));
   /* revert the hardware address for sender and reciever.*/
    memcpy(new_arp_hdr -> ar_sha, sr_interface -> addr,ETHER_ADDR_LEN);
    memcpy(new_arp_hdr -> ar_tha,  arp_hdr -> ar_sha,ETHER_ADDR_LEN);
   /* assign the rest variable for arp header.*/
    new_arp_hdr -> ar_hrd = arp_hdr -> ar_hrd;
    new_arp_hdr -> ar_pro = arp_hdr -> ar_pro;
    new_arp_hdr -> ar_hln = arp_hdr -> ar_hln;
    new_arp_hdr -> ar_pln = arp_hdr -> ar_pln;
   /* convert the op code to big endian.*/
    new_arp_hdr -> ar_op = htons(arp_op_reply);
   /* revert the ip address for sender and receiever.*/
    new_arp_hdr -> ar_sip = sr_interface -> ip;
    new_arp_hdr -> ar_tip = arp_hdr -> ar_sip;
    sr_send_packet(sr,back_packet,length_new_packet,sr_interface->name);
    free(back_packet);
}

void handle_arp_reply(struct sr_instance* sr,uint8_t * packet,unsigned int len,char* interface) {
  /*get the cache for sr.*/
  struct sr_arpcache *sr_cache = &sr->cache;
  sr_arp_hdr_t *arp_hdr = (sr_arp_hdr_t*) (packet + sizeof(sr_ethernet_hdr_t));
  uint32_t target_ip = arp_hdr -> ar_tip;
  sr_arpcache_lookup(sr_cache,target_ip);
  

}

void handle_ip(struct sr_instance* sr,uint8_t * packet,unsigned int len,char* interface) {
    assert(sr);
    assert(packet);
    assert(interface);

    printf("Receiving IP Package.\n");
    print_hdr_ip(packet + sizeof(sr_ethernet_hdr_t));
    
    /*Get the ethernet header.*/
    sr_ethernet_hdr_t *eth_hdr = get_ethrnet_hdr(packet);
    
    /* Get the ip header.*/
    sr_ip_hdr_t *ip_hdr = (sr_ip_hdr_t*) (packet + sizeof(sr_ethernet_hdr_t));
    /* Error checking of assigning ip header.*/
    if (!ip_hdr) {
        fprintf(stderr, "Assigning ip header error.\n");
        exit(0);
    }
    
    /*Checking checksum.*/
    uint16_t checksum = ip_hdr -> ip_sum;
    ip_hdr -> ip_sum = 0;
    if (cksum(ip_hdr, sizeof(sr_ip_hdr_t) != checksum)) {
        fprintf(stderr, "Wrong checksum.\n");
    }
    ip_hdr -> ip_sum = checksum;
    
    /* Find if the destination of package is this router.*/
    struct sr_if *dest_interface = sr_get_interface_by_ip(sr, ip_hdr -> ip_dst);
     
    /*Packet destination is this router.*/
    if (dest_interface) {
      printf("Packet for this router.\n");

      switch (ip_hdr -> ip_p)
      {
      case ip_protocol_icmp:
        
        break;
      
      case ip_protocol_tcp:

        break;

      case ip_protocol_udp:

        break;
      default:
        printf("Cannot handle packet protocol.\n");
        break;
      }
    } else {  /*Packet destination is elsewhere.*/
      printf("Packet not for this router.\n");
      
    }

    
    
}

/*Function that assign packet to ethrnet header.*/
sr_ethernet_hdr_t * get_ethrnet_hdr(uint8_t * packet) {
    assert(packet);
    
    sr_ethernet_hdr_t *ethernet_hdr = (sr_ethernet_hdr_t*) packet;
    /* Error checking.*/
    if (!ethernet_hdr) {
        fprintf(stderr, "Assigning ethernet header error.\n");
        exit(0);
    }
    return ethernet_hdr;
}






