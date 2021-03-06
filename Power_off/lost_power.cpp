/*
 * read_GPIO.cpp
 *
 *  Created on: Nov 25, 2021
 *      Author: bergma
 */

#include <iostream>
#include <fstream>
#include <string>
#include <string.h>
#include <sstream>
#include <unistd.h>
#include <stdlib.h>
#include <chrono>
#include <stdio.h>
#include <sstream>
#include <thread>
#include "MQTTClient.h"
#include "read_conf.h"

using namespace std;

#define VAL "/value"
#define DIRECTION "/direction"
#define PATH "/sys/class/gpio/gpio"
#define OUTPUT "/home/debian/"
//Please replace the following address with the address of your server
/*
#define ADDRESS    	"ssl://ec2-3-122-253-158.eu-central-1.compute.amazonaws.com:8883"
#define CLIENTID   	"Beagle1"

#define USER		"gokart"
#define PASSWD		"lgiekGLQ!drbn_lir439"
#define CA_PATH		"/home/debian/Gokart_CAN_API/client_certs/ca.crt"
#define CERT_PATH	"/home/debian/Gokart_CAN_API/client_certs/client.crt"
#define KEY_PATH	"/home/debian/Gokart_CAN_API/client_certs/client.key"
*/
#define QOS        	1
#define TIMEOUT    	10000L

char ONmsg[] = "Power_goes_ON";
char LOSTmsg[] = "Power_was_lost_Waiting";
char OFFmsg[] = "Power_still_off_shutting_down";
char REGAINEDmsg[] = "Power_restored";

MQTTClient_deliveryToken token;
MQTTClient_message pubmsg = MQTTClient_message_initializer;

fstream openGPIO(string path, string filename, string type);
int getValue(fstream &GPIO);
int setValue(fstream &GPIO, string value);
void check_power_pin(int pin_number, int delay, MQTTClient cli, MQTTClient_connectOptions * conn_opts);
void cli_reconnect(MQTTClient * cli, MQTTClient_connectOptions * conn_opts);

int main(int argc, char **argv)
{
	/******* Read configuration file ******/
	read_configuration();
	printf("topic: %s\n",topic);
	sleep(5);
	
	fstream GPIO_setup;
	fstream GPIO_file;
	// Convert pin number to text
	char pin_text[] = "87";
	// Open direction file
	GPIO_setup = openGPIO((string) PATH + (string) pin_text, (string) DIRECTION,"out");
	// Set contents to out
	setValue(GPIO_setup, "in");
	// Close the file again
	GPIO_setup.close();
	GPIO_file = openGPIO((string) PATH + (string) pin_text, (string) VAL, "in");
	
	/*********** Create MQTT Client ************/
	MQTTClient client;
	cout << "Attempting to create MQTT Client" << endl;
	MQTTClient_connectOptions conn_opts = MQTTClient_connectOptions_initializer;
	MQTTClient_SSLOptions ssl_opts = MQTTClient_SSLOptions_initializer;

	MQTTClient_create(&client, host, client_id, MQTTCLIENT_PERSISTENCE_NONE,NULL);
	conn_opts.keepAliveInterval = 2000;
	conn_opts.cleansession = 1;
	//opts.username = USER;
	//opts.password = PASSWD;
	conn_opts.ssl = &ssl_opts;
	conn_opts.ssl->enableServerCertAuth = 1;
	// conn_opts.ssl->struct_version = 1;
	conn_opts.ssl->CApath = ca_path;
	conn_opts.ssl->keyStore = cert_path;
	//conn_opts.ssl->trustStore = CERT_PATH;
	conn_opts.ssl->privateKey = key_path;
	conn_opts.ssl->sslVersion = MQTT_SSL_VERSION_TLS_1_2;
	
	/*********** Necessary when using self-signed certificates *************/
	if (ssl_check == "NO")
	{
		conn_opts.ssl->enableServerCertAuth = 0;
	}	

	/*********** Connect MQTT Client ************/
	int rc;
	cout << "Connecting MQTT client" << endl;
	while ((rc = MQTTClient_connect(client, &conn_opts)) != MQTTCLIENT_SUCCESS)
	{
		cout << "Failed to connect, return code " << rc
				<< ", trying again in 2 sec" << endl;
		if (getValue(GPIO_file) == 0)
		{
			/********** Shut the system down *********/
			system("shutdown -h now");
		}
		sleep(2);
	}
	GPIO_file.close();
	
	/********** Load message to be sent *********/
	pubmsg.payload = ONmsg;
	pubmsg.payloadlen = strlen(ONmsg);
	pubmsg.qos = QOS;
	pubmsg.retained = 0;
	/********** Publish message ***********/
	MQTTClient_publishMessage(client, topic, &pubmsg, &token);

	/********** Waiting on acknowledge from broker *********/
	cout << "Delivering ON msg to server" << endl;
	rc = MQTTClient_waitForCompletion(client, token, TIMEOUT);
	cout << "Message with token " << (int) token << " delivered." << endl;

	check_power_pin(87, 500, client, &conn_opts);

	/********** Program should never reach this point *********/
	MQTTClient_disconnect(client, 10000);
	MQTTClient_destroy(&client);

	return 0;
}

fstream openGPIO(string path, string filename, string type)
{
	fstream fs;
	if (type == "in")
	{
		fs.open(path + filename, std::fstream::in);
	} else if (type == "out" || type == "w")
	{
		fs.open(path + filename, std::fstream::out);
	}

	if (!fs.is_open())
	{
		perror("GPIO: read failed to open file ");
	}
	return fs;
}

int getValue(fstream &GPIO)
{

	string input;
	// Clear fail bit
	GPIO.clear();
	// Seek to beginning of file
	GPIO.seekg(0);
	// Read contents
	GPIO >> input;

	if (input == "0")
	{
		return 0;
	} else
	{
		return 1;
	}
}
int setValue(fstream &GPIO, string value)
{
	// Clear fail bit
	GPIO.clear();
	// Seek to beginning of file
	GPIO.seekg(0);
	// write contents
	GPIO << value;

	return 1;
}

void check_power_pin(int pin_number, int delay, MQTTClient cli, MQTTClient_connectOptions * conn_opts)
{
	fstream GPIO_setup;
	fstream GPIO_out;
	fstream GPIO_file;
	fstream LOGFILE;
	char message[128];
	char buf[32];
	time_t givemetime = time(NULL);
	int rc = 0;
	int MQTT_return;
	int keep_alive_count = 0;
	
	LOGFILE.open("/home/debian/power_log",std::fstream::app);

	char pin_text[4];
	// Convert pin number to text
	sprintf(pin_text, "%d", pin_number);
	// Open direction file
	GPIO_setup = openGPIO((string) PATH + (string) pin_text, (string) DIRECTION,
			"out");
	// Set contents to out
	setValue(GPIO_setup, "in");
	// Close the file again
	GPIO_setup.close();
	GPIO_file = openGPIO((string) PATH + (string) pin_text, (string) VAL, "in");

	while (1)
	{
		while (1)
		{
			/********** Keep connection open *********/
			MQTTClient_yield();
			

			/********** Check if power was lost *********/
			if (getValue(GPIO_file) == 0)
			{
				/********** Load message to be sent *********/
				cout << "Delivering Power goes off msg to server" << endl;
				pubmsg.payload = LOSTmsg;
				pubmsg.payloadlen = strlen(LOSTmsg);
				pubmsg.qos = QOS;
				pubmsg.retained = 0;
				/********** Publish message *********/
				MQTT_return = MQTTClient_publishMessage(cli, topic, &pubmsg,
						&token);
				cout << "Publish message return value: " << MQTT_return << endl;
				if (MQTT_return != 0)
				{
					cli_reconnect(&cli, conn_opts);
					MQTT_return = MQTTClient_publishMessage(cli, topic, &pubmsg,
						&token);
					cout << "Republish message return value: " << MQTT_return << endl;
				}
				/********** Wait for publish complete *********/
				MQTTClient_waitForCompletion(cli, token, TIMEOUT);
				cout << "Message with token " << (int) token << " delivered."
						<< endl;
				break;
			}
			usleep(delay * 1000);
		}
		/********** Wait a little to ensure that power is actually off *********/
		usleep(delay * 5000);
		/********** Check power state again *********/
		if (getValue(GPIO_file) == 0)
		{
			cout << "Shutting down" << endl;
			LOGFILE << "Shutting down" << endl;
			/********** Load message to be sent *********/
			pubmsg.payload = OFFmsg;
			pubmsg.payloadlen = strlen(OFFmsg);
			pubmsg.qos = QOS;
			pubmsg.retained = 0;
			/********** Publish message *********/
			MQTT_return = MQTTClient_publishMessage(cli, topic, &pubmsg,
					&token);
			cout << "Publish message return value: " << MQTT_return << endl;
			LOGFILE << "Publish message return value: " << MQTT_return << endl;
			
			if (MQTT_return != 0)
			{
				cli_reconnect(&cli, conn_opts);
				MQTT_return = MQTTClient_publishMessage(cli, topic, &pubmsg,
					&token);
				cout << "Republish message return value: " << MQTT_return << endl;
			}
			
			/********** Wait for publish complete *********/
			cout << "Wait for complete" << endl;
			LOGFILE << "Wait for complete" << endl;
			MQTTClient_waitForCompletion(cli, token, TIMEOUT);
			cout << "Delivery completed" << endl;
			LOGFILE << "Delivery completed" << endl;
			/********** Shut the system down *********/
			LOGFILE << "Sleeping for 5 seconds" << endl;
			sleep(5);
			LOGFILE << "Shutting down" << endl;
			system("shutdown -h now");
		}
		/********** Power has returned *********/
		else
		{
			/********** Load message to be sent *********/
			cout << "Power restored" << endl;
			pubmsg.payload = REGAINEDmsg;
			pubmsg.payloadlen = strlen(REGAINEDmsg);
			pubmsg.qos = QOS;
			pubmsg.retained = 0;
			/********** Publish message *********/
			MQTT_return = MQTTClient_publishMessage(cli, topic, &pubmsg,
					&token);
			cout << "Publish message return value: " << MQTT_return << endl;
			
			if (MQTT_return != 0)
			{
				cli_reconnect(&cli, conn_opts);
				MQTT_return = MQTTClient_publishMessage(cli, topic, &pubmsg,
					&token);
				cout << "Republish message return value: " << MQTT_return << endl;
			}
			
			/********** Wait for publish complete *********/
			MQTTClient_waitForCompletion(cli, token, TIMEOUT);
		}
	}

}
void cli_reconnect(MQTTClient * cli, MQTTClient_connectOptions * conn_opts)
{
	int rc;
	MQTTClient_disconnect(cli, 200);
	rc = MQTTClient_connect(&cli, conn_opts);
	cout << "Reconnected to client with return value: " << rc << endl;
	return;
}
