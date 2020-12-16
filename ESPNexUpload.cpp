/**
 * @file NexUpload.cpp
 *
 * The implementation of uploading tft file for nextion displays. 
 * 
 * Original version (a part of https://github.com/itead/ITEADLIB_Arduino_Nextion)
 * @author  Chen Zengpeng (email:<zengpeng.chen@itead.cc>)
 * @date    2016/3/29
 * @copyright 
 * Copyright (C) 2014-2015 ITEAD Intelligent Systems Co., Ltd.
 *
 * This program is free software: you can redistribute it and/or modify  
 * it under the terms of the GNU General Public License as published by  
 * the Free Software Foundation, version 3.
 *
 * This program is distributed in the hope that it will be useful, but 
 * WITHOUT ANY WARRANTY; without even the implied warranty of 
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU 
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 */
#include "ESPNexUpload.h"



ESPNexUpload::ESPNexUpload(uint32_t upload_baudrate){
    _upload_baudrate = upload_baudrate;
}



bool ESPNexUpload::connect(){
    #if defined ESP8266
        yield();
    #endif

    //dbSerialBegin(115200);
	_printInfoLine(F("serial tests & connect"));

	_setRunningMode();

   /* if(!_echoTest("mystop_yesABC")){
        statusMessage = F("echo test failed");
        _printInfoLine(statusMessage);
        return false;
    }*/

    if(!_handlingSleepAndDim()){
        statusMessage = F("handling sleep and dim settings failed");
        _printInfoLine(statusMessage);
        return false;
    }

	if(!_setPrepareForFirmwareUpdate(_upload_baudrate)){
        statusMessage = F("modifybaudrate error");
        _printInfoLine(statusMessage);
        return false;
    }

	return true;
}



bool ESPNexUpload::prepareUpload(uint32_t file_size){
    _undownloadByte = file_size;
	return this->connect();
}








bool ESPNexUpload::_setPrepareForFirmwareUpdate(uint32_t upload_baudrate){

    #if defined ESP8266
        yield();
    #endif

    String response = String(""); 
    String cmd = String("");

	cmd = F("00");
	sendCommand(cmd.c_str());
	delay(0.1);

	recvRetStringReal(response, 800, true); // normal response time is 400ms

    String filesize_str = String(_undownloadByte,10);
    String baudrate_str = String(upload_baudrate);
    cmd = "whmi-wri " + filesize_str + "," + baudrate_str + ",0";

	sendCommand(cmd.c_str());

	// Without flush, the whmi command will NOT transmitted by the ESP in the current baudrate
	// because switching to another baudrate (nexSerialBegin command) has an higher prio.
	// The ESP will first jump to the new 'upload_baudrate' and than process the serial 'transmit buffer'
	// The flush command forced the ESP to wait until the 'transmit buffer' is empty
	sendFlush();
	
	
    _printInfoLine(F("changing upload baudrate..."));
    _printInfoLine(String(upload_baudrate));

	recvRetStringReal(response, 800, true); // normal response time is 400ms
	
	// The Nextion display will, if it's ready to accept data, send a 0x05 byte.
    if(response.indexOf(0x05) != -1)
    { 
		_printInfoLine(F("preparation for firmware update done"));
        return 1;
    }else { 
		_printInfoLine(F("preparation for firmware update failed"));
		return 0;
	}
}



void ESPNexUpload::setUpdateProgressCallback(THandlerFunction value){
	_updateProgressCallback = value;
}



bool ESPNexUpload::upload(const uint8_t *file_buf, size_t buf_size){

    #if defined ESP8266
        yield();
    #endif

    uint8_t c;
    uint8_t timeout = 0;
    String string = String("");
    for(uint16_t i = 0; i < buf_size; i++){

		// Users must split the .tft file contents into 4096 byte sized packets with the final partial packet size equal to the last remaining bytes (<4096 bytes).
		if(_sent_packets == 4096){

			// wait for the Nextion to return its 0x05 byte confirming reception and readiness to receive the next packets
			recvRetStringReal(string,500,true);   

			if(string.indexOf(0x05) != -1){ 
			

				// reset sent packets counter
				_sent_packets = 0;

				// reset receive String
				string = "";
			}else{
				if(timeout >= 8){
					statusMessage = F("serial connection lost");
					_printInfoLine(statusMessage);
					return false;
				}

				timeout++;
			}

			// delay current byte
			i--;

		}else{

			// read buffer
			c = file_buf[i];

			// write byte to nextion over serial
			//DA CAPIRE
			//nexSerial1.write(c);

			// update sent packets counter
			_sent_packets++;
		}
    }

    return true;  
}



bool ESPNexUpload::upload(Stream &myFile){
    #if defined ESP8266
        yield();
    #endif

	// create buffer for read
	uint8_t buff[2048] = { 0 };

	// read all data from server
	while(_undownloadByte > 0 || _undownloadByte == -1){

		// get available data size
		size_t size = myFile.available();

		if(size){
			// read up to 2048 byte into the buffer
			int c = myFile.readBytes(buff, ((size > sizeof(buff)) ? sizeof(buff) : size));

			// Write the buffered bytes to the nextion. If this fails, return false.
			if(!this->upload(buff, c)){
				return false;
			}else{
				if(_updateProgressCallback){
					_updateProgressCallback();
				}
			}

			if(_undownloadByte > 0) {
				_undownloadByte -= c;
			}
		}
		delay(1);
	}

    return true;  
}



void ESPNexUpload::softReset(void){
    // soft reset nextion device
	sendCommand("rest");
}



void ESPNexUpload::end(){

    // wait for the nextion to finish internal processes
    delay(1600);

	// soft reset the nextion
	this->softReset();

	// reset sent packets counter
	_sent_packets = 0;

    statusMessage = F("upload ok");
    _printInfoLine(statusMessage + F("\r\n"));
}



void ESPNexUpload::_setRunningMode(void) {
	String cmd = String("");
	delay (100);
	cmd = F("runmod=2");
	sendCommand(cmd.c_str());
	delay(60);
}



/*bool ESPNexUpload::_echoTest(String input) {
	String cmd = String("");
	String response = String("");

	cmd = "print \"" + input + "\"";
	sendCommand(cmd.c_str());

	uint32_t duration_ms = calculateTransmissionTimeMs(cmd) * 2 + 10; // times 2  (send + receive) and 10 ms extra
	recvRetString(response,duration_ms); 

	return (response.indexOf(input) != -1);
}
*/

uint32_t ESPNexUpload::calculateTransmissionTimeMs(String message){
	// In general, 1 second (s) = 1000 (10^-3) millisecond (ms) or 
	//             1 second (s) = 1000 000 (10^-6) microsecond (us). 
	// To calculate how much microsecond one BIT of data takes with a certain baudrate you have to divide 
	// the baudrate by one second. 
	// For example 9600 baud = 1000 000 us / 9600 ≈ 104 us
	// The time to transmit one DATA byte (if we use default UART modulation) takes 10 bits. 
	// 8 DATA bits and one START and one STOP bit makes 10 bits. 
	// In this example (9600 baud) a byte will take 1041 us to send or receive. 
	// Multiply this value by the length of the message (number of bytes) and the total transmit/ receive time
	// is calculated.

    uint32_t duration_one_byte_us = 10000000 / _baudrate; // 1000 000 * 10 bits / baudrate
    uint16_t nr_of_bytes = message.length() + 3;          // 3 times 0xFF byte
    uint32_t duration_message_us = nr_of_bytes * duration_one_byte_us;
    uint32_t return_value_ms = duration_message_us / 1000;

	_printInfoLine("calculated transmission time: " + String(return_value_ms) + " ms");
	return return_value_ms;
}


void ESPNexUpload::_printInfoLine(String line){
	//dbSerialPrint(F("Status     info: "));
	//if(line.length() != 0)
//		dbSerialPrintln(line);
}