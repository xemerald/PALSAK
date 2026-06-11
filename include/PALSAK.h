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

/* */
#include <u7186EX/7186e.h>

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
 * @name The size of the buffer used to received the data from broadcast
 *
 */
#define COMMBUF_SIZE     32
#define RECVBUF_SIZE     256
#define PREBUF_SIZE      512

/**
 * @name String length of those responses from the other P-Alert
 *
 */
#define MAC_STRING        17  /* Exclude the null-terminator */
#define IPV4_STRING       15  /* Exclude the null-terminator */
#define DABSIZE_STRING     2  /* Exclude the null-terminator */
#define RSVSIZE_STRING     3  /* Exclude the null-terminator */
#define PSERIAL_STRING    11  /* Exclude the null-terminator */
#define CVALUE_STRING     35  /* Exclude the null-terminator */
#define NETCONFIG_FORMAT  "%03u.%03u.%03u.%03u-%02u  %03u.%03u.%03u.%03u  "

/**
 * @name All used file's information
 *
 */
#define BLOCK_0_FILE_NAME    "block_0.ini"
#define FTP_INFO_FILE_NAME   "ftp_info.ini"
#define AGENT_EXE_FILE_NAME  "AGENT.EXE"
#define AUTOEXEC_FILE_NAME   "autoexec.bat"
#define DISK_FOR_FIRMWARE    DISKB

/**
 * @brief
 *
 */
#define DIGIT_LIMIT_TABLE \
	X( DIGIT_LIMIT_TO_TWO   , '2' ) \
	X( DIGIT_LIMIT_TO_THREE , '3' ) \
	X( DIGIT_LIMIT_TO_FIVE  , '5' ) \
	X( DIGIT_LIMIT_TO_NINE  , '9' )

#define X(a, b) a,
typedef enum {
	DIGIT_LIMIT_TABLE
	DIGIT_LIMIT_COUNT
} DIGIT_LIMITS;
#undef X

/**
 * @brief Workflow unit define
 *
 */
#define STRATEGY_UPD_FW      0x0000
#define STRATEGY_CHK_MAC     0x0001
#define STRATEGY_UPL_FW      0x0002
#define STRATEGY_GET_NET     0x0004
#define STRATEGY_SET_NET     0x0008
#define STRATEGY_WRT_BL0     0x0010
#define STRATEGY_CHK_CON     0x0020
#define STRATEGY_FAC_OVR     0x0040
#define STRATEGY_FAC_APL     0x0080
#define STRATEGY_SET_DHCP    0x0100
#define STRATEGY_CHK_CN      0x0200

/**
 * @brief Workflow combaination
 *
 */
#define WORKFLOWS_TABLE \
	X( WORKFLOW_0, STRATEGY_UPD_FW                                                                             ) \
	X( WORKFLOW_1, STRATEGY_GET_NET | STRATEGY_CHK_MAC                                                         ) \
	X( WORKFLOW_2, STRATEGY_GET_NET | STRATEGY_WRT_BL0 | STRATEGY_CHK_CON | STRATEGY_CHK_MAC                   ) \
	X( WORKFLOW_3, STRATEGY_GET_NET | STRATEGY_UPL_FW | STRATEGY_WRT_BL0 | STRATEGY_CHK_CON | STRATEGY_CHK_MAC ) \
	X( WORKFLOW_4, STRATEGY_CHK_CN                                                                             ) \
	X( WORKFLOW_5, STRATEGY_SET_NET | STRATEGY_CHK_MAC                                                         ) \
	X( WORKFLOW_6, STRATEGY_SET_DHCP | STRATEGY_CHK_MAC                                                        ) \
	X( WORKFLOW_7, STRATEGY_FAC_OVR | STRATEGY_CHK_CON                                                         ) \
	X( WORKFLOW_8, STRATEGY_FAC_APL | STRATEGY_CHK_CON                                                         )

#define X(a, b) a,
typedef enum {
	WORKFLOWS_TABLE
	WORKFLOW_COUNT
} WORKFLOWS;
#undef X

/**
 * @brief Agent commands
 *
 */
#define AGENT_COMMANDS_TABLE \
	X( AGENT_COMMAND_WBLOCK0 , "wblock0"    , 7 ) \
	X( AGENT_COMMAND_CHECKCON, "checkcon"   , 8 ) \
	X( AGENT_COMMAND_OVERRIDE, "override %s", 8 ) \
	X( AGENT_COMMAND_FACTORY , "factory %s" , 7 ) \
	X( AGENT_COMMAND_DHCP    , "dhcp %s"    , 4 ) \
	X( AGENT_COMMAND_QUIT    , "quit"       , 4 )

#define X(a, b, c) a,
typedef enum {
	AGENT_COMMANDS_TABLE
	AGENT_COMMAND_COUNT
} AGENT_COMMANDS;
#undef X

/**
 * @brief Cirtical factory parameters
 *
 */
#define FACTORY_PARAMS_TABLE \
	X( FACTORY_PARAM_SERIAL , "serial" , 6, 0x5b | 0x80, 0x00 ) \
	X( FACTORY_PARAM_CVALUE0, "cvalue0", 7, 0x4e, 0x7e | 0x80 ) \
	X( FACTORY_PARAM_CVALUE1, "cvalue1", 7, 0x4e, 0x30 | 0x80 )

#define X(a, b, c, d, e) a,
typedef enum {
	FACTORY_PARAMS_TABLE
	FACTORY_PARAM_COUNT
} FACTORY_PARAMS;
#undef X

/**
 * @brief Firmware clip slots
 *
 */
#define FWCLIP_SLOTS_TABLE \
	X( FWCLIP_SLOT_0, FILE_DATA *, __slot_0, GetFileInfoByNo_AB(DISKA, 0) ) \
	X( FWCLIP_SLOT_1, FILE_DATA *, __slot_1, GetFileInfoByNo_AB(DISKB, 0)) \
	X( FWCLIP_SLOT_2, FILE_DATA *, __slot_2, GetFileInfoByNo_AB(DISKA, 1) ) \
	X( FWCLIP_SLOT_3, FILE_DATA *, __slot_3, GetFileInfoByNo_AB(DISKB, 1) )

#define X(a, b, c, d) a,
typedef enum {
	FWCLIP_SLOTS_TABLE
	FWCLIP_SLOT_COUNT
} FWCLIP_SLOTS;
#undef X

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

/**
 * @brief
 *
 */
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
 * @brief
 *
 */
#define EEPROM_SETTING_CONFIG_BLOCK  0x00
#define EEPROM_FACTORY_CONFIG_BLOCK  0x01
#define EEPROM_NETWORK_CONFIG_BLOCK  0x02

/**
 * @brief Temporary P-Alert Network setting storage information
 *
 */
#define EEPROM_NETWORK_TMP_ADDR    0x00
#define EEPROM_NETWORK_DEF_ADDR    0x18
#define EEPROM_NETWORK_SET_LENGTH  18

/**
 * @brief
 *
 */
#define EEPROM_BYTE_PER_LINE     8
#define EEPROM_SET_END_ADDR      0x70
#define EEPROM_SERIAL_ADDR       0x08
#define EEPROM_SERIAL_LENGTH     2
#define EEPROM_CVALUE_1_ADDR     0x30
#define EEPROM_CVALUE_0_ADDR     0x36
#define EEPROM_CVALUE_LENGTH     6
#define EEPROM_CVALUES_LENGTH    12
#define EEPROM_OPMODE_ADDR       0x3c
#define EEPROM_OPMODE_LENGTH     2
#define EEPROM_ALLOWIP_0_ADDR    0x5a
#define EEPROM_ALLOWIP_0_LENGTH  4

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
