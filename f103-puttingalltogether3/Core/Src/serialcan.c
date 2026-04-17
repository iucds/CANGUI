/*
 * serialcan.c
 *
 *  Created on: 24 Mar 2026
 *      Author: de en
 */

#include "main.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>

#define DESTIDSHIFT 0
#define CMDSHIFT 8
#define CPUEDGESHIFT 16
#define FORMATSHIFT 18
#define TYPESHIFT 24
#define CMDCATSHIFT 20

uint8_t lookup[3][256][20] = {{" "," ","Q%x","S%x","R%x"},{"C%x%xN%x%x","P%x","H%x","t%xB%xl%xr%x"},{"I%xo%x"}};

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
	MSG MyOwnState;
	MSG Neighbours[4];
}REG;

void hello(){
	HAL_GPIO_TogglePin(GPIOA,GPIO_PIN_5);
	HAL_Delay(100);
}

void generateRegistry(int seed,int MAXIDs,CAN_TxHeaderTypeDef txHeader,uint32_t canMailbox,CAN_HandleTypeDef *hcan){
	REG Registry[MAXIDs];
	srand(seed);
	//initialize all neighbour id as 99 and initialise own state
	for (int i = 0;i<MAXIDs;i++){
	  Registry[i].MyOwnState.ID = i;
	  Registry[i].MyOwnState.MyType = rand()%3;
	  Registry[i].Neighbours[0].ID = 99;
	  Registry[i].Neighbours[1].ID = 99;
	  Registry[i].Neighbours[2].ID = 99;
	  Registry[i].Neighbours[3].ID = 99;
	}
	//randomly decides to be connected and if chooses to be connected will choose a random module to set as neighbour as well as a random edge
	for (int i=0;i<MAXIDs;i++){
	  for (int j=0;j<4;j++){
		  if (Registry[i].Neighbours[j].ID == 99){
			  if (rand()%2){
				  uint8_t destid = rand()%50;
				  while (destid==i||(Registry[destid].Neighbours[0].ID!=99&&Registry[destid].Neighbours[1].ID!=99&&Registry[destid].Neighbours[2].ID!=99&&Registry[destid].Neighbours[3].ID!=99)) destid = rand()%50;
				  uint8_t destedge = rand()%4;
				  while (Registry[destid].Neighbours[destedge].ID!=99) destedge = rand()%4;
				  //set the edge and facing of the 2 connected edges in the registry
				  Registry[i].Neighbours[j] = Registry[destid].MyOwnState;
				  Registry[i].Neighbours[j].FacingE = destedge;
				  Registry[destid].Neighbours[destedge] = Registry[i].MyOwnState;
				  Registry[destid].Neighbours[destedge].FacingE = j;
			  }
		  }
	  }
	}
	//registry fill complete
	//continue with packaging the data into can before sending out
	for (int i=0;i<MAXIDs;i++){
	  REG current_register = Registry[i];
	  MSG mymodule = current_register.MyOwnState;
	  uint32_t canID = mymodule.MyType<<TYPESHIFT|0<<CMDCATSHIFT|0<<FORMATSHIFT|0<<CPUEDGESHIFT|1<<CMDSHIFT|mymodule.ID<<DESTIDSHIFT;
	  uint8_t candata[8] = {0,0,0,0,0,0,0,0};
	  candata[0] = current_register.Neighbours[0].ID;
	  candata[1] = current_register.Neighbours[1].ID;
	  candata[2] = current_register.Neighbours[2].ID;
	  candata[3] = current_register.Neighbours[3].ID;
	  candata[4] = current_register.Neighbours[0].FacingE;
	  candata[5] = current_register.Neighbours[1].FacingE;
	  candata[6] = current_register.Neighbours[2].FacingE;
	  candata[7] = current_register.Neighbours[3].FacingE;
	  // printf("%i %i %i %i %i %i %i %i %i %i\n",i,canID,candata[0],candata[1],candata[2],candata[3],candata[4],candata[5],candata[6],candata[7]);
	  txHeader.ExtId = canID;
	  if (HAL_CAN_AddTxMessage(hcan, &txHeader, candata, &canMailbox) != HAL_OK)
	  {
		  /* Transmission request Error */
		Error_Handler();
	  }
	  HAL_Delay(1);
	}
}

void serial_to_can(uint8_t UART1_rxBuffer[],UART_HandleTypeDef huart1,CAN_TxHeaderTypeDef txHeader,uint32_t canMailbox,CAN_HandleTypeDef *hcan){
	uint8_t values[] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}; //T,E,i,C,N,P,H,t,B,I,o,Q,L,R,F,Z,D,S,r
	uint8_t states[] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}; //T,E,i,C,N,P,H,t,B,I,o,Q,L,R,F,Z,D,S,r
	uint8_t* to_store;
	uint32_t id;
	uint8_t data[8]={0,0,0,0,0,0,0,0};

	for (int j=0;UART1_rxBuffer[j]!=13;j++){
		switch (UART1_rxBuffer[j]){
			case 'T':
				to_store = &values[0];
				states[0] = 1;
				break;
			case 'E':
				to_store = &values[1];
				states[1] = 1;
				break;
			case 'i':
				to_store = &values[2];
				states[2] = 1;
				break;
			case 'C':
				to_store = &values[3];
				states[3] = 1;
				break;
			case 'N':
				to_store = &values[4];
				states[4] = 1;
				break;
			case 'P':
				to_store = &values[5];
				states[5] = 1;
				break;
			case 'H':
				to_store = &values[6];
				states[6] = 1;
				break;
			case 't':
				to_store = &values[7];
				states[7] = 1;
				break;
			case 'B':
				to_store = &values[8];
				states[8] = 1;
				break;
			case 'I':
				to_store = &values[9];
				states[9] = 1;
				break;
			case 'o':
				to_store = &values[10];
				states[10] = 1;
				break;
			case 'Q':
				to_store = &values[11];
				states[11] = 1;
				break;
			case 'l':
				to_store = &values[12];
				states[12] = 1;
				break;
			case 'R':
				to_store = &values[13];
				states[13] = 1;
				break;
			case 'F':
				to_store = &values[14];
				states[14] = 1;
				break;
			case 'Z':
				to_store = &values[15];
				states[15] = 1;
				break;
			case 'D':
				to_store = &values[16];
				states[16] = 1;
				break;
			case 'S':
				to_store = &values[17];
				states[17] = 1;
				break;
			case 'r':
				to_store = &values[18];
				states[18] = 1;
				break;
			default:
				if (UART1_rxBuffer[j]-48<10) *to_store = *to_store*16+UART1_rxBuffer[j]-48;
				else *to_store = *to_store*16+UART1_rxBuffer[j]-87;
				break;
		}
	}
	id = (uint32_t)values[0]<<TYPESHIFT|((uint32_t)values[1]-1)<<CPUEDGESHIFT|(uint32_t)values[2]<<DESTIDSHIFT;
	if (states[3]||states[4]){ //topomux
		id |= 1<<CMDCATSHIFT|1<<FORMATSHIFT|0<<CMDSHIFT;
		data[0] = values[3];
		data[1] = values[4];
	}
	else if (states[5]){ // cold
		id |= 1<<CMDCATSHIFT|0<<FORMATSHIFT|1<<CMDSHIFT;
		data[0] = values[5];
	}
	else if (states[6]){ //hot
		id |= 1<<CMDCATSHIFT|0<<FORMATSHIFT|2<<CMDSHIFT;
		data[0] = values[6];
	}
	else if (states[7]||states[8]||states[12]||states[18]){ //tb voltage
		id |= 1<<CMDCATSHIFT|0<<FORMATSHIFT|3<<CMDSHIFT;
		data[0] = values[7];
		data[1] = values[8];
		data[2] = values[12];
		data[3] = values[18];
	}
	else if (states[9]||states[10]){ //io
		id |= 2<<CMDCATSHIFT|0<<FORMATSHIFT|0<<CMDSHIFT;
		data[0] = values[9];
		data[1] = values[10];
	}
	else if (states[11]) { //q
		id |= 0<<CMDCATSHIFT|0<<FORMATSHIFT|2<<CMDSHIFT;
		data[0] = 0;
		data[1] = 0;
	}
	else if (states[17]){ //s
		id |= 0<<CMDCATSHIFT|0<<FORMATSHIFT|3<<CMDSHIFT;
		data[0] = values[17];
	}
	else if (states[13]){ //r
		id |= 0<<CMDCATSHIFT|0<<FORMATSHIFT|4<<CMDSHIFT;
		data[0] = values[13];
	}
	txHeader.ExtId = id;
	if (HAL_CAN_AddTxMessage(hcan, &txHeader, data, &canMailbox) != HAL_OK)
	{
  /* Transmission request Error */
	    Error_Handler();
    }
	HAL_GPIO_TogglePin(GPIOA,GPIO_PIN_5);
}

void can_to_serial(CAN_HandleTypeDef *hcan,CAN_RxHeaderTypeDef rxHeader,uint8_t canRX[],UART_HandleTypeDef huart[]){
	if (HAL_CAN_GetRxMessage(hcan, CAN_RX_FIFO0, &rxHeader, canRX) != HAL_OK)
	    {
		  Error_Handler();
	    }
		uint32_t id;
		if (rxHeader.IDE == CAN_ID_EXT) id = rxHeader.ExtId;
		else id = rxHeader.StdId;
		uint8_t format     = (id>>FORMATSHIFT)&0x3;
		uint8_t d[16] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
		char output[100];
		strcpy(output,"T%xE%xi%x");
		switch (format){
	        case 0: //bytes
	            for (int j=0;j<8;j++){
	                d[j] = canRX[j];
	            }
	            break;
	        case 1: //nibble
	            for (int j=0;j<8;j++){
	                d[2*j] = canRX[j]>>4;
	                d[2*j+1] = canRX[j]&0xF;
	            }
	            break;
	    }
		uint8_t i      = id&0xFF;
		uint8_t cmd    = (id>>CMDSHIFT)&0xFF;
		uint8_t e      = ((id>>CPUEDGESHIFT)&0x3);
		uint8_t cmdcat = (id>>CMDCATSHIFT)&0xF;
		uint8_t type   = id>>TYPESHIFT;
		strcat(output,lookup[cmdcat&0x7][cmd]);
		strcat(output,"\r");
		char finalout[200];
		sprintf(finalout,output,type,e+1,i,d[0],d[1],d[2],d[3],d[4],d[5],d[6],d[7],d[8],d[9],d[10],d[11],d[12],d[13],d[14],d[15]);
		HAL_UART_Transmit(&huart[e], (uint8_t*)finalout, strlen(finalout), 20);
		HAL_GPIO_TogglePin(GPIOA,GPIO_PIN_5);
}
