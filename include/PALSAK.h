/**
 * @file PALSAK.h
 * @author Benjamin Ming Yang (b98204032@gmail.com) in Department of Geology of National Taiwan University
 * @brief
 * @version 0.1
 * @date 2022-12-30
 *
 * @copyright Copyright (c) 2022
 *
 */
#ifndef __PALSAK_H__
#define __PALSAK_H__

#ifdef __cplusplus
extern "C" {
#endif

/* */
#define TAIWAN_TIME_ZONE  8
/* Address to accept any incoming messages */
#define INADDR_ANY        0x00000000
/* Address to send to all hosts */
#define INADDR_BROADCAST  0xffffffff
/* The port used to listen the broadcast messages from palert */
#define LISTEN_PORT       54321
/* The port used to listen the broadcast messages from palert */
#define CONTROL_BIND_PORT 12345
/* The port used to send the control messages */
#define CONTROL_PORT      23
/* */
#define NETWORK_OPERATION_RETRY  3
/* The size of the buffer used to received the data from broadcast */
#define COMMBUF_SIZE     32
#define RECVBUF_SIZE     256
#define PREBUF_SIZE      512
/* String length of those responses from the other palert */
#define MAC_STRING       17  /* Exclude the null-terminator */
#define IPV4_STRING      15  /* Exclude the null-terminator */
#define DABSIZE_STRING    2  /* Exclude the null-terminator */
#define RSVSIZE_STRING    3  /* Exclude the null-terminator */
#define PSERIAL_STRING   11  /* Exclude the null-terminator */
#define CVALUE_STRING    35  /* Exclude the null-terminator */
/* Workflow unit define */
#define STRATEGY_CHK_MAC     0x01
#define STRATEGY_UPL_FW      0x02
#define STRATEGY_GET_NET     0x04
#define STRATEGY_SET_NET     0x08
#define STRATEGY_SET_EEP     0x10
#define STRATEGY_CHK_EEP     0x20
#define STRATEGY_CHK_CN      0x40
#define STRATEGY_UPD_FW      0x80
/* Workflow combaination */
#define WORKFLOW_0           STRATEGY_UPD_FW
#define WORKFLOW_1           STRATEGY_CHK_MAC | STRATEGY_GET_NET
#define WORKFLOW_2           STRATEGY_CHK_MAC | STRATEGY_GET_NET | STRATEGY_UPL_FW
#define WORKFLOW_3           STRATEGY_CHK_MAC | STRATEGY_GET_NET | STRATEGY_SET_EEP
#define WORKFLOW_4           STRATEGY_CHK_CN
#define WORKFLOW_5           STRATEGY_CHK_MAC | STRATEGY_SET_NET
#define WORKFLOW_6           STRATEGY_CHK_MAC | STRATEGY_GET_NET | STRATEGY_CHK_EEP
/* The checking result of func. */
#define NORMAL   0
#define ERROR   -1
/* The functional mode of "CheckPalertDisk()" */
#define CHECK  0
#define RESET  1
/* The default(normal) disk size partition of the palert */
#define DISKA_SIZE   3
#define DISKB_SIZE   4
#define RESERVE_SIZE 0
#define DISK_RAM     3
/* The response from the other palert when receiving data bytes */
#define ACK   6
#define NAK  21
/* */
#define NETWORK_TEMPORARY  0
#define NETWORK_DEFAULT    1
/* Temporary Palert Network setting storage information */
#define EEPROM_NETWORK_SET_BLOCK   0x02
#define EEPROM_NETWORK_TMP_ADDR    0x00
#define EEPROM_NETWORK_DEF_ADDR    0x10
#define EEPROM_NETWORK_SET_LENGTH  12
/* */
#define EEPROM_BYTE_PER_LINE  8
#define EEPROM_SET_END_ADDR   0x70
#define EEPROM_SERIAL_ADDR    0x08
#define EEPROM_SERIAL_LENGTH  2
#define EEPROM_CVALUE_ADDR    0x30
#define EEPROM_CVALUE_LENGTH  12
/* */
#define EEPROM_SET_TOTAL_LENGTH  EEPROM_SET_END_ADDR

#ifdef __cplusplus
}
#endif
#endif
