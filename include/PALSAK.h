/**
 * @file PALSAK.h
 * @author Benjamin Ming Yang (b98204032@gmail.com) in Department of Geology of National Taiwan University
 * @brief
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

/**
 * @brief The time zone setting for local
 *
 */
#define TAIWAN_TIME_ZONE  8

/**
 * @brief Address to accept any incoming messages
 *
 */
#define INADDR_ANY        0x00000000

/**
 * @brief Address to send to all hosts
 *
 */
#define INADDR_BROADCAST  0xffffffff

/**
 * @brief The port used to listen the broadcast messages from P-Alert
 *
 */
#define LISTEN_PORT       54321

/**
 * @brief The port used to listen the broadcast messages from P-Alert
 *
 */
#define CONTROL_BIND_PORT 12345

/**
 * @brief The port used to send the control messages
 *
 */
#define CONTROL_PORT      23

/**
 * @brief
 *
 */
#define NETWORK_OPERATION_RETRY  3

/**
 * @brief The size of the buffer used to received the data from broadcast
 *
 */
#define COMMBUF_SIZE     32
#define RECVBUF_SIZE     256
#define PREBUF_SIZE      512

/**
 * @brief String length of those responses from the other P-Alert
 *
 */
#define MAC_STRING       17  /* Exclude the null-terminator */
#define IPV4_STRING      15  /* Exclude the null-terminator */
#define DABSIZE_STRING    2  /* Exclude the null-terminator */
#define RSVSIZE_STRING    3  /* Exclude the null-terminator */
#define PSERIAL_STRING   11  /* Exclude the null-terminator */
#define CVALUE_STRING    35  /* Exclude the null-terminator */

/**
 * @brief Workflow unit define
 *
 */
#define STRATEGY_CHK_MAC     0x01
#define STRATEGY_UPL_FW      0x02
#define STRATEGY_GET_NET     0x04
#define STRATEGY_SET_NET     0x08
#define STRATEGY_SET_EEP     0x10
#define STRATEGY_CHK_EEP     0x20
#define STRATEGY_CHK_CN      0x40
#define STRATEGY_UPD_FW      0x80

/**
 * @brief Workflow combaination
 *
 */
#define WORKFLOW_0           STRATEGY_UPD_FW
#define WORKFLOW_1           STRATEGY_CHK_MAC | STRATEGY_GET_NET
#define WORKFLOW_2           STRATEGY_CHK_MAC | STRATEGY_GET_NET | STRATEGY_SET_EEP
#define WORKFLOW_3           STRATEGY_CHK_MAC | STRATEGY_GET_NET | STRATEGY_SET_EEP | STRATEGY_UPL_FW
#define WORKFLOW_4           STRATEGY_CHK_CN
#define WORKFLOW_5           STRATEGY_CHK_MAC | STRATEGY_SET_NET
#define WORKFLOW_6           STRATEGY_CHK_MAC | STRATEGY_GET_NET | STRATEGY_CHK_EEP

/**
 * @brief The checking result of func.
 *
 */
#define NORMAL   0
#define ERROR   -1

/**
 * @brief The functional mode of "CheckP-AlertDisk()"
 *
 */
#define CHECK  0
#define RESET  1

/**
 * @brief The default(normal) disk size partition of the P-Alert
 *
 */
#define DISKA_SIZE   3
#define DISKB_SIZE   4
#define RESERVE_SIZE 0
#define DISK_RAM     3

/**
 * @brief The response from the other P-Alert when receiving data bytes
 *
 */
#define ACK   6
#define NAK  21

/**
 * @brief
 *
 */
#define NETWORK_TEMPORARY  0
#define NETWORK_DEFAULT    1

/**
 * @brief Temporary P-Alert Network setting storage information
 *
 */
#define EEPROM_NETWORK_SET_BLOCK   0x02
#define EEPROM_NETWORK_TMP_ADDR    0x00
#define EEPROM_NETWORK_DEF_ADDR    0x18
#define EEPROM_NETWORK_SET_LENGTH  18

/**
 * @brief
 *
 */
#define EEPROM_BYTE_PER_LINE  8
#define EEPROM_SET_END_ADDR   0x70
#define EEPROM_SERIAL_ADDR    0x08
#define EEPROM_SERIAL_LENGTH  2
#define EEPROM_CVALUE_ADDR    0x30
#define EEPROM_CVALUE_LENGTH  12
#define EEPROM_OPMODE_ADDR    0x3c
#define EEPROM_OPMODE_LENGTH  2

/**
 * @brief 40118 Operation mode(OPMODE) flags
 *
 */
#define OPMODE_BITS_GBT_INTENSITY       0x0001
#define OPMODE_BITS_GAS_MODE            0x0002
#define OPMODE_BITS_INTENSITY_VECTOR    0x0004
#define OPMODE_BITS_CONNECT_TCP0        0x0008
#define OPMODE_BITS_CONNECT_NTP         0x0010
#define OPMODE_BITS_MODE_DHCP           0x0020
#define OPMODE_BITS_CONNECT_TCP1        0x0040
#define OPMODE_BITS_DISCONNECT_SANLIEN  0x0080
#define OPMODE_BITS_CONNECT_FTE_D04     0x0100
#define OPMODE_BITS_MMI_INTENSITY       0x0200
#define OPMODE_BITS_KMA_INTENSITY       0x0400
#define OPMODE_BITS_MODBUS_TCP_CLIENT   0x8000

/**
 * @brief
 *
 */
#define EEPROM_SET_TOTAL_LENGTH  EEPROM_SET_END_ADDR

/**
 * @brief
 *
 */
#define SWAP_WORD_ASM(__WORD) \
		{ _AX = (__WORD); _asm { xchg al, ah }; (__WORD) = _AX; }

#ifdef __cplusplus
}
#endif
#endif
