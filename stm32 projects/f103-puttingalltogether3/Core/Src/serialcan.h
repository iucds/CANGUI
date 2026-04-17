/*
 * serialcan.h
 *
 *  Created on: 25 Mar 2026
 *      Author: de en
 */

#ifndef SRC_SERIALCAN_H_
#define SRC_SERIALCAN_H_

typedef struct {
	uint8_t newMsg;
	uint8_t ID;	// runing number from 1
	uint8_t io;	// I/O of module
	uint8_t Edg;	// Sender's Edge [0 to 3]
	uint8_t FacingE;// My (slave) Facing Edge at that edge [0 to 3]
	uint8_t MyType;	// My (slave) Type
	uint8_t CMD; 	// Command Query or Set [unused]
	uint8_t CmdNum;	// Command number [unused]
}MSG;

typedef struct {
	MSG MyOwnState; //all the information of the current module
	MSG Neighbours[4]; //all the informations of the adjacent modules to the current module
}REG;

void hello(); //simple built in led toggle with 100ms delay for testing

//To generate a registry of modules that are connected together, use the generateRegistry function
void generateRegistry(
		int seed, //the seed for randomizing the parameters of the registry and configuration
		int MAXIDs, //the number of modules that will exist in the generated registry
		CAN_TxHeaderTypeDef txHeader, //txHeader of the CAN peripheral that the configuration info will be sent through
		uint32_t canMailbox, //canMailbox of the CAN peripheral that the configuration info will be sent through
		CAN_HandleTypeDef *hcan //CAN peripheral that the configuration info will be sent through
		);

//To convert serial commands into CAN messages, use the serial_to_can function
void serial_to_can(
		uint8_t UART1_rxBuffer[], //buffer in which stores the serial command that is to be converted
		UART_HandleTypeDef huart1, //UART peripheral that will receive the serial command
		CAN_TxHeaderTypeDef txHeader, //txHeader of the CAN peripheral that will send the converted message
		uint32_t canMailbox, //canMailbox of the CAN peripheral that will send the converted message
		CAN_HandleTypeDef *hcan); //CAN peripheral that will output the converted message

//To convert CAN comands into serial messages, use the can_to_serial function
void can_to_serial(
		CAN_HandleTypeDef *hcan, //CAN peripheral that will receive the CAN command
		CAN_RxHeaderTypeDef rxHeader, //rxHeader of the CAN peripheral that will receive the CAN command
		uint8_t canRX[], //Data of the received CAN command
		UART_HandleTypeDef huart[]); //UART peripheral that will output the converted message

#endif /* SRC_SERIALCAN_H_ */
