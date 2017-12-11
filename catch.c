#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <signal.h>
#include <unistd.h>

#include "catch.h"


//here's the plan....
//open up the file and look to see if we have a mailbox request (REQ_MAILBOX)
//if a request is present (!0) change from last reading
//read the mailbox (REQ_DATA)
//send info to the header validation code (our slave address and first 2 bytes zero byte sum)
//if that is good, send the command data to the NetFun Handler
//        Net function handler will call the assocoated command handler
//        Fill out responce (ACK_DATA) based on command completion( either Good (0x00) with data, or !0 for error)
//Set the ACK_MAILBOX to the same value from the REQ_MAILBOX
//read REQ_MAILBOX for 0 to indicate BC has read responce
//write ACK_MAILBOX to 0 to complete the command sequence
//rense and repeat




//commands to do
//Write -> iDrac => Seq: 0x6C NetFn/CMD CS-OEM    SC_BMC_SET_SENSOR_INFO                                   :[C0 20 70 6C 15 20 FE 2A 00 17 00 00 00 00 00 07 00 EC 00 E4 00 DB 00 00 00 00 00 00 00 00 00 00 00 03 02 00 00 08 55 56 56 56 55 56 55 56 55 55 55 56 EF]
//Write -> MC    => Seq: 0x6C NetFn/CMD cs-oem    SC_BMC_SET_SENSOR_INFO                                   :[C4 CC 20 6C 15 00 00 1B 1A FF FF 2C]
//Write -> iDrac => Seq: 0x78 NetFn/CMD CS-OEM    SC_BMC_SET_CHASSIS_POWER_READINGS                        :[C0 20 70 78 2F 03 AB 02 55 00 02 FF FF FF E5]
//Write -> MC    => Seq: 0x78 NetFn/CMD cs-oem    SC_BMC_SET_CHASSIS_POWER_READINGS                        :[C4 CC 20 78 2F 00 39]
//Write -> iDrac => Seq: 0xA4 NetFn/CMD Trans     IPMI_CMD_GET_LAN_CONFIG_PARA (5=MAC 3=IP)                :[30 B0 70 A4 02 01 03 00 00 E6]
//Write -> MC    => Seq: 0xA4 NetFn/CMD trans     IPMI_CMD_GET_LAN_CONFIG_PARA (5=MAC 3=IP)                :[34 5C 20 A4 02 00 11 C0 A8 11 C1 EF]

void sig_term_handler(int signum, siginfo_t *info, void *ptr)
{
    write(STDERR_FILENO, SIGTERM_MSG, sizeof(SIGTERM_MSG));
	system(KILL_SLAVE);
    exit(0);	
}

void catch_sigterm()
{
    static struct sigaction _sigact;

    memset(&_sigact, 0, sizeof(_sigact));
    _sigact.sa_sigaction = sig_term_handler;
    _sigact.sa_flags = SA_SIGINFO;

    sigaction(SIGTERM, &_sigact, NULL);
}


unsigned char checkSumData(char *buffer, int length){
int x;
int checkSum = 0;

	for(x = 0; x < length;x++)
		checkSum += buffer[x];

	return (0x100 - (checkSum & 0xff));
}

//checks to see if a new mailbox value is set (new comamnd or complete sequence
//if there is a non-zero mailbox copy the data (with slave address prependd) and set the lenght
int checkMailBox(){

FILE *fptr;
char mailBox;
int returnValue = 0;

	 reqBuffer.reqPacket.reqDataPktSize = 0;	
	 fptr = fopen(FILE_NAME,"rb");
	 fseek(fptr, REQ_MAILBOX, SEEK_SET);
	 mailBox = fgetc( fptr );
	 reqBuffer.reqPacket.reqDataPktSize = fgetc( fptr );
	 if(mailBox != reqBuffer.reqPacket.lastMailBox){
		reqBuffer.reqPacket.lastMailBox = mailBox;
		if(mailBox != 0x00){
	 		fseek(fptr, REQ_DATA, SEEK_SET);
			reqBuffer.reqPacket.BMCi2cAddress = MYSLAVEADDRESS;
			reqBuffer.reqPacket.BCi2cAddress = BCSLAVEADDRESS;
			fgets(&reqBuffer.reqPacket.netFunc_LUN, MAILBOXDATASIZE , fptr);
	 		reqBuffer.reqPacket.reqDataPktSize++;	//+1 for adding in my slave address

		}
		returnValue = 1;
	 }else{
		returnValue = 0;
	 }

	fclose(fptr);
	return returnValue;
}


//this will check the header and payload area
//0- OK
//1- header failed
//2-payload failed
int validateComamndData(char *buffer, int length){
int x;
int chksum = 0;

	for(x = 0;x<3;x++){
		chksum += buffer[x];
	}
   	if(chksum & 0xff)
   		return 1;
	for(;x<length;x++){				//take up where we left off
		chksum += buffer[x];
	}
   	if(chksum & 0xff)
   		return 2;

   return 0;


}

int processGetID(){
//this is just a dummy responce... It will be filled by calls to teh Dbus layer i think)

	ackBuffer.ackPacket.BCi2cAddress= reqBuffer.reqPacket.BCi2cAddress;
	ackBuffer.ackPacket.netFunc_LUN = reqBuffer.reqPacket.netFunc_LUN + 0x04; 		//turn into a reponce netFun 0x1c;
	ackBuffer.ackPacket.headerCheckSum = (0x200 - ackBuffer.ackPacket.BCi2cAddress - ackBuffer.ackPacket.netFunc_LUN) & 0xff;
	ackBuffer.ackPacket.BMCi2cAddress = MYSLAVEADDRESS;
	ackBuffer.ackPacket.sequence = reqBuffer.reqPacket.sequence;
	ackBuffer.ackPacket.command = reqBuffer.reqPacket.command;
	ackBuffer.ackPacket.completionCode = 0x00; 	
	ackBuffer.ackPacket.payLoad[0] = 0x20;
	ackBuffer.ackPacket.payLoad[1] = 0x81;
	ackBuffer.ackPacket.payLoad[2] = 0x03;
	ackBuffer.ackPacket.payLoad[3] = 0x00;
	ackBuffer.ackPacket.payLoad[4] = 0x02;
	ackBuffer.ackPacket.payLoad[5] = 0xdf;
	ackBuffer.ackPacket.payLoad[6] = 0xa2;
	ackBuffer.ackPacket.payLoad[7] = 0x02;
	ackBuffer.ackPacket.payLoad[8] = 0x00;
	ackBuffer.ackPacket.payLoad[9] = 0x00;
	ackBuffer.ackPacket.payLoad[10] = 0x01;
	ackBuffer.ackPacket.payLoad[11] = 0x00;
	ackBuffer.ackPacket.payLoad[12] = 0x43;
	ackBuffer.ackPacket.payLoad[13] = 0x00;
	ackBuffer.ackPacket.payLoad[14] = 0x00;	
	ackBuffer.ackPacket.payLoad[15] = checkSumData(&ackBuffer.ackPacket.BMCi2cAddress, 19);	 

	ackBuffer.ackPacket.reqDataPktSize = 22;


return 0;
}

int processGetPWM(){

//Write -> iDrac => Seq: 0x70 NetFn/CMD CS-OEM    SC_BMC_GET_PWM                                           :[C0 20 70 70 8C 94]
//Write -> MC    => Seq: 0x70 NetFn/CMD cs-oem    SC_BMC_GET_PWM                                           :[C4 CC 20 70 8C 00 39 00 AB]
	ackBuffer.ackPacket.BCi2cAddress= reqBuffer.reqPacket.BCi2cAddress;
	ackBuffer.ackPacket.netFunc_LUN = reqBuffer.reqPacket.netFunc_LUN + 0x04; 		//turn into a reponce netFun 0x1c;
	ackBuffer.ackPacket.headerCheckSum = (0x200 - ackBuffer.ackPacket.BCi2cAddress - ackBuffer.ackPacket.netFunc_LUN) & 0xff;
	ackBuffer.ackPacket.BMCi2cAddress = MYSLAVEADDRESS;
	ackBuffer.ackPacket.sequence = reqBuffer.reqPacket.sequence;
	ackBuffer.ackPacket.command = reqBuffer.reqPacket.command;
	ackBuffer.ackPacket.completionCode = 0x00; 	
	ackBuffer.ackPacket.payLoad[0] = 0x20;
	ackBuffer.ackPacket.payLoad[1] = 0x39;
	ackBuffer.ackPacket.payLoad[2] = 0x00;
	ackBuffer.ackPacket.payLoad[3] = checkSumData(&ackBuffer.ackPacket.BMCi2cAddress, 7);	 

	ackBuffer.ackPacket.reqDataPktSize = 10;


return 0;


}

int processGetPower(){

//Write -> iDrac => Seq: 0x74 NetFn/CMD GRP-EXTN  IPMI_DCMI_CMD_GET_POWER_READING                          :[B0 30 70 74 02 DC 01 00 00 3D]
//Write -> MC    => Seq: 0x74 NetFn/CMD grp-extn  IPMI_DCMI_CMD_GET_POWER_READING                          :[B4 DC 20 74 02 00 DC 54 00 07 00 20 01 41 00 A1 FF 41 59 E8 03 00 00 40 6C]
	ackBuffer.ackPacket.BCi2cAddress= reqBuffer.reqPacket.BCi2cAddress;
	ackBuffer.ackPacket.netFunc_LUN = reqBuffer.reqPacket.netFunc_LUN + 0x04; 		//turn into a reponce netFun 0x1c;
	ackBuffer.ackPacket.headerCheckSum = (0x200 - ackBuffer.ackPacket.BCi2cAddress - ackBuffer.ackPacket.netFunc_LUN) & 0xff;
	ackBuffer.ackPacket.BMCi2cAddress = MYSLAVEADDRESS;
	ackBuffer.ackPacket.sequence = reqBuffer.reqPacket.sequence;
	ackBuffer.ackPacket.command = reqBuffer.reqPacket.command;
	ackBuffer.ackPacket.completionCode = 0x00; 	
	ackBuffer.ackPacket.payLoad[0] = 0xdc;
	ackBuffer.ackPacket.payLoad[1] = 0x54;
	ackBuffer.ackPacket.payLoad[2] = 0x00;
	ackBuffer.ackPacket.payLoad[3] = 0x07;
	ackBuffer.ackPacket.payLoad[4] = 0x00;
	ackBuffer.ackPacket.payLoad[5] = 0x20;
	ackBuffer.ackPacket.payLoad[6] = 0x01;
	ackBuffer.ackPacket.payLoad[7] = 0x41;
	ackBuffer.ackPacket.payLoad[8] = 0x00;
	ackBuffer.ackPacket.payLoad[9] = 0xa1;
	ackBuffer.ackPacket.payLoad[10] = 0xff;
	ackBuffer.ackPacket.payLoad[11] = 0x41;
	ackBuffer.ackPacket.payLoad[12] = 0x59;
	ackBuffer.ackPacket.payLoad[13] = 0xe8;
	ackBuffer.ackPacket.payLoad[14] = 0x03;
	ackBuffer.ackPacket.payLoad[15] = 0x00;
	ackBuffer.ackPacket.payLoad[16] = 0x00;
	ackBuffer.ackPacket.payLoad[17] = 0x40;
	ackBuffer.ackPacket.payLoad[18] = checkSumData(&ackBuffer.ackPacket.BMCi2cAddress, 22);	 

	ackBuffer.ackPacket.reqDataPktSize = 25;


return 0;


}

int processNicInfo(){

//Write -> iDrac => Seq: 0xA4 NetFn/CMD Trans     IPMI_CMD_GET_LAN_CONFIG_PARA (5=MAC 3=IP)                :[30 B0 70 A4 02 01 03 00 00 E6]
//Write -> MC    => Seq: 0xA4 NetFn/CMD trans     IPMI_CMD_GET_LAN_CONFIG_PARA (5=MAC 3=IP)                :[34 5C 20 A4 02 00 11 C0 A8 11 C1 EF]
	ackBuffer.ackPacket.BCi2cAddress= reqBuffer.reqPacket.BCi2cAddress;
	ackBuffer.ackPacket.netFunc_LUN = reqBuffer.reqPacket.netFunc_LUN + 0x04; 		//turn into a reponce netFun 0x1c;
	ackBuffer.ackPacket.headerCheckSum = (0x200 - ackBuffer.ackPacket.BCi2cAddress - ackBuffer.ackPacket.netFunc_LUN) & 0xff;
	ackBuffer.ackPacket.BMCi2cAddress = MYSLAVEADDRESS;
	ackBuffer.ackPacket.sequence = reqBuffer.reqPacket.sequence;
	ackBuffer.ackPacket.command = reqBuffer.reqPacket.command;
	ackBuffer.ackPacket.completionCode = 0x00; 	
	ackBuffer.ackPacket.payLoad[0] = 0xdc;
	ackBuffer.ackPacket.payLoad[1] = 0x54;
	ackBuffer.ackPacket.payLoad[2] = 0x00;
	ackBuffer.ackPacket.payLoad[3] = 0x07;
	ackBuffer.ackPacket.payLoad[4] = 0x00;
	ackBuffer.ackPacket.payLoad[5] = 0x20;
	ackBuffer.ackPacket.payLoad[6] = 0x01;
	ackBuffer.ackPacket.payLoad[7] = 0x41;
	ackBuffer.ackPacket.payLoad[8] = 0x00;
	ackBuffer.ackPacket.payLoad[9] = 0xa1;
	ackBuffer.ackPacket.payLoad[10] = 0xff;
	ackBuffer.ackPacket.payLoad[11] = 0x41;
	ackBuffer.ackPacket.payLoad[12] = 0x59;
	ackBuffer.ackPacket.payLoad[13] = 0xe8;
	ackBuffer.ackPacket.payLoad[14] = 0x03;
	ackBuffer.ackPacket.payLoad[15] = 0x00;
	ackBuffer.ackPacket.payLoad[16] = 0x00;
	ackBuffer.ackPacket.payLoad[17] = 0x40;
	ackBuffer.ackPacket.payLoad[18] = checkSumData(&ackBuffer.ackPacket.BMCi2cAddress, 22);	 

	ackBuffer.ackPacket.reqDataPktSize = 25;


return 0;


}



int processNotSupported(){

	ackBuffer.ackPacket.BCi2cAddress= reqBuffer.reqPacket.BCi2cAddress;
	ackBuffer.ackPacket.netFunc_LUN = reqBuffer.reqPacket.netFunc_LUN + 0x04; 		//turn into a reponce netFun 0x1c;
	ackBuffer.ackPacket.headerCheckSum = (0x200 - ackBuffer.ackPacket.BCi2cAddress - ackBuffer.ackPacket.netFunc_LUN) & 0xff;
	ackBuffer.ackPacket.BMCi2cAddress = MYSLAVEADDRESS;
	ackBuffer.ackPacket.sequence = reqBuffer.reqPacket.sequence;
	ackBuffer.ackPacket.command = reqBuffer.reqPacket.command;
	ackBuffer.ackPacket.completionCode = 0xc1; 	
	ackBuffer.ackPacket.payLoad[0] = checkSumData(&ackBuffer.ackPacket.BMCi2cAddress, 4);	 

	ackBuffer.ackPacket.reqDataPktSize = 7;

}


int processNetFun_CMD(){

int netFuncCmd = (reqBuffer.reqPacket.netFunc_LUN << 8) + reqBuffer.reqPacket.command;
int returnVal = 1;

	printf("\t\tprocessNetFun_CMD()\n");
	switch(netFuncCmd){
		case IPMI_CMD_GET_DEVICE_ID: 
			printf("\t\t\tIPMI_CMD_GET_DEVICE_ID\n");
			processGetID();
		break;
		case SC_BMC_GET_PWM:
			printf("\t\t\tSC_BMC_GET_PWM\n");
			processGetPWM();
		break;
		case IPMI_DCMI_CMD_GET_POWER_READING:
			printf("\t\t\tIPMI_DCMI_CMD_GET_POWER_READING\n");
			processGetPower();
		break;
		default:
			printf("%04X not supported yet\n",netFuncCmd);
			processNotSupported();
			//this should load up an unsupport responce
			returnVal = 0;
	}			 

return returnVal = 1;
}

int writeDataACK(){
FILE *fptr;

	 printf("\t\t\t\twriteDataACK()\n");
	
	 ackBuffer.ackPacket.lastMailBox = reqBuffer.reqPacket.lastMailBox;
	 fptr = fopen(FILE_NAME,"rb+");
	 fseek(fptr, ACK_DATA, SEEK_SET);
	 fwrite(&ackBuffer.ackPacket.netFunc_LUN,ackBuffer.ackPacket.reqDataPktSize,1,fptr);	//load up the answer
	 fseek(fptr, ACK_MAILBOX, SEEK_SET);
	 fwrite(ackBuffer.buffer,2,1,fptr);	
	 
	  	
	fclose(fptr);
	return 0;

}

int finishHandShake(){

FILE *fptr;

	 printf("finishHandShake()\n");
	 ackBuffer.ackPacket.lastMailBox = reqBuffer.reqPacket.lastMailBox;
	 ackBuffer.ackPacket.reqDataPktSize = 0;
	 fptr = fopen(FILE_NAME,"rb+");
	 fseek(fptr, ACK_MAILBOX, SEEK_SET);
	 fwrite(ackBuffer.buffer,2,1,fptr);	
	fclose(fptr);
	//clear out all the buffers
	memset(ackBuffer.ackPacket.payLoad,0xff, 122);
	memset(reqBuffer.reqPacket.payLoad,0xff, 123);
	return 0;
}


//return value will be BC heart beat... later
long heartBeat = 0;
int upDateHeartBeat(){
FILE *fptr;


	 heartBeat++;
	 fptr = fopen(FILE_NAME,"rb+");
	 fseek(fptr, BMC_HB, SEEK_SET);
	 fwrite(&heartBeat,sizeof(heartBeat),1,fptr);
	  	
	fclose(fptr);
	return 44;

}

int initFile(){
FILE *fptr;


	 heartBeat++;
	 fptr = fopen(FILE_NAME,"rb+");
	 fseek(fptr, VERSION, SEEK_SET);
	 fwrite("001X",4,1,fptr);
	  	
	fclose(fptr);
	return 44;

}



void main()
{
int x;
int id;
int chkSumOK = 0;
long sleepCount = 0;

	id = fork();
	if(id == 0){	//this is the child

		//init the reg buffer....
		memset(reqBuffer.buffer,0,sizeof(reqBuffer.buffer));
		memset(ackBuffer.buffer,0,sizeof(ackBuffer.buffer));
		system(CREATE_SLAVE);
    	initFile();
    	catch_sigterm();
    	while(1){			
    		if(checkMailBox()){
				if(reqBuffer.reqPacket.lastMailBox != 0){
					printf("\nPossible New Comamnd [%02x][%02x]\n\t",reqBuffer.reqPacket.lastMailBox,reqBuffer.reqPacket.reqDataPktSize);
					for(x=0;x<reqBuffer.reqPacket.reqDataPktSize;x++){	
						printf("%02X ",reqBuffer.buffer[2+x]);
					}
					printf("\n");
					if(chkSumOK = validateComamndData(&reqBuffer.reqPacket.BMCi2cAddress, reqBuffer.reqPacket.reqDataPktSize)){
						printf("reqBuffer checksum validation failed [%02x]\n",chkSumOK);
						//this will then load up an error packer
					}else{
						//the command is packet checksums out OK, start processin the command
						if(processNetFun_CMD()){
							writeDataACK();
						}
				   }

				}else{
					printf("Comamnd Complete Sequence [%02x][%02x]\n\t",reqBuffer.reqPacket.lastMailBox,reqBuffer.reqPacket.reqDataPktSize);
					finishHandShake();
					printf("Task pid : %d\n",getpid());
				}
    		}
			if((sleepCount % 200) == 0)	//??? 20 didn't do it, the sleep may not be very calibrated hit the headbeat once a second
				upDateHeartBeat();

    		sleep(.05);
			sleepCount++;
    	}
    }else{
		printf("Let the parrent die....\n");
    }	
}

